#include "Maze.h"

#include <QDebug>
#include <QString>
#include <QQueue>

#include "Assert.h"
#include "Logging.h"
#include "MazeChecker.h"
#include "MazeFileType.h"
#include "MazeFileUtilities.h"
#include "MazeUtilities.h"
#include "Param.h"
#include "Tile.h"

namespace mms {

Maze* Maze::fromFile(const QString& path) {
    BasicMaze basicMaze;
    try {
        basicMaze = MazeFileUtilities::load(path);
    }
    catch (const std::exception& e) {
        qWarning().nospace()
            << "Unable to initialize maze from file " << path << ": "
            << QString(e.what()) << ".";
        return nullptr;
    }
    return new Maze(basicMaze);
}

Maze* Maze::fromAlgo(const QByteArray& bytes) {
    // TODO: MACK - dedup with fromFile
    // TODO: MACK - rename this to fromBytes
    BasicMaze basicMaze;
    try {
        basicMaze = MazeFileUtilities::loadBytes(bytes);
    }
    catch (const std::exception& e) {
        qWarning().nospace()
            << "Unable to initialize maze from bytes " << bytes << ": "
            << QString(e.what()) << ".";
        return nullptr;
    }
    return new Maze(basicMaze);
}

Maze::Maze(BasicMaze basicMaze) {

    // Validate the maze
    MazeValidity validity = MazeChecker::checkMaze(basicMaze);

    // Check to see if it's a valid maze
    m_isValidMaze = (
        validity == MazeValidity::EXPLORABLE ||
        validity == MazeValidity::OFFICIAL
    );
    m_isOfficialMaze = (
        validity == MazeValidity::OFFICIAL
    );

    // TODO: MACK - fix maze saving
    /*
    // Optionally save the maze
    if (P()->saveGeneratedMaze()) {
        MazeFileType type = STRING_TO_MAZE_FILE_TYPE().value(P()->generatedMazeType());
        QString generatedMazeFilePath = Directory::get()->getResMazeDirectory() +
            P()->generatedMazeFile() + "." + MAZE_FILE_TYPE_TO_SUFFIX().value(type);
        bool success = false; // MazeFileUtilities::save(basicMaze, generatedMazeFilePath, type);
        if (success) {
            qInfo().noquote().nospace()
                << "Maze saved to \"" << generatedMazeFilePath << "\".";
        }
        else {
            qWarning().noquote().nospace()
                << "Unable to save maze to \"" << generatedMazeFilePath << "\".";
        }
    }
    */

    // TODO: MACK - make it possible to do rotate and mirroring on the fly
    /*
    // Mirror and rotate the maze
    if (P()->mazeMirrored()) {
        basicMaze = mirrorAcrossVertical(basicMaze);
        qInfo().noquote().nospace()
            << "Mirroring the maze across the vertical.";
    }
    for (int i = 0; i < P()->mazeRotations(); i += 1) {
        basicMaze = rotateCounterClockwise(basicMaze);
        qInfo().noquote().nospace()
            << "Rotating the maze counter-clockwise (" << i + 1 << ").";
    }
    */

    // Load the maze given by the maze generation algorithm
    m_maze = initializeFromBasicMaze(basicMaze);
}

int Maze::getWidth() const {
    return m_maze.size();
}

int Maze::getHeight() const {
    return (0 < m_maze.size() ? m_maze.at(0).size() : 0);
}

bool Maze::withinMaze(int x, int y) const {
    return 0 <= x && x < getWidth() && 0 <= y && y < getHeight();
}

const Tile* Maze::getTile(int x, int y) const {
    ASSERT_TR(withinMaze(x, y));
    return &m_maze.at(x).at(y);
}

int Maze::getMaximumDistance() const {
    int max = 0;
    for (int x = 0; x < getWidth(); x += 1) {
        for (int y = 0; y < getHeight(); y += 1) {
            if (max < getTile(x, y)->getDistance()) {
                max = getTile(x, y)->getDistance();
            }
        }
    }
    return max;
}

bool Maze::isValidMaze() const {
    return m_isValidMaze;
}

bool Maze::isOfficialMaze() const {
    return m_isOfficialMaze;
}

bool Maze::isCenterTile(int x, int y) const {
    const auto centerPositions = 
        MazeUtilities::getCenterPositions(getWidth(), getHeight());
    return centerPositions.contains({x, y});
}

Direction Maze::getOptimalStartingDirection() const {
    if (getHeight() == 0) {
        return Direction::NORTH;
    }
    if (getTile(0, 0)->isWall(Direction::NORTH) &&
        !getTile(0, 0)->isWall(Direction::EAST)) {
        return Direction::EAST;
    }
    return Direction::NORTH;
}

QVector<QVector<Tile>> Maze::initializeFromBasicMaze(const BasicMaze& basicMaze) {
    // TODO: MACK - assert valid here
    QVector<QVector<Tile>> maze;
    for (int x = 0; x < basicMaze.size(); x += 1) {
        QVector<Tile> column;
        for (int y = 0; y < basicMaze.at(x).size(); y += 1) {
            Tile tile;
            tile.setPos(x, y);
            for (Direction direction : DIRECTIONS()) {
                tile.setWall(direction, basicMaze.at(x).at(y).value(direction));
            }
            tile.initPolygons(basicMaze.size(), basicMaze.at(x).size());
            column.push_back(tile);
        }
        maze.push_back(column);
    }
    maze = setTileDistances(maze);
    return maze;
}

BasicMaze Maze::mirrorAcrossVertical(const BasicMaze& basicMaze) {
    static QMap<Direction, Direction> verticalOpposites {
        {Direction::NORTH, Direction::NORTH},
        {Direction::EAST, Direction::WEST},
        {Direction::SOUTH, Direction::SOUTH},
        {Direction::WEST, Direction::EAST},
    };
    // TODO: MACK - test this
    BasicMaze mirrored;
    for (int x = 0; x < basicMaze.size(); x += 1) {
        QVector<BasicTile> column;
        for (int y = 0; y < basicMaze.at(x).size(); y += 1) {
            BasicTile tile;
            for (Direction direction : DIRECTIONS()) {
                tile.insert(
                    direction,
                    basicMaze
                        .at(basicMaze.size() - 1 - x)
                        .at(y)
                        .value(verticalOpposites.value(direction))
                );
            }
            column.push_back(tile);
        }
        mirrored.push_back(column);
    }
    return mirrored; 
}

BasicMaze Maze::rotateCounterClockwise(const BasicMaze& basicMaze) {
    BasicMaze rotated;
    for (int x = 0; x < basicMaze.size(); x += 1) {
        QVector<BasicTile> row;
        for (int y = basicMaze.at(x).size() - 1; y >= 0; y -= 1) {
            BasicTile tile;
            int rotatedTileX = basicMaze.at(x).size() - 1 - y;
            tile.insert(Direction::NORTH, basicMaze.at(x).at(y).value(Direction::EAST));
            tile.insert(Direction::EAST, basicMaze.at(x).at(y).value(Direction::SOUTH));
            tile.insert(Direction::SOUTH, basicMaze.at(x).at(y).value(Direction::WEST));
            tile.insert(Direction::WEST, basicMaze.at(x).at(y).value(Direction::NORTH));
            if (rotated.size() <= rotatedTileX) {
                rotated.push_back({tile});
            }
            else {
                rotated[rotatedTileX].push_back(tile);
            }
        }
    }
    return rotated;
}

QVector<QVector<Tile>> Maze::setTileDistances(QVector<QVector<Tile>> maze) {

    // TODO: MACK - dedup some of this with hasNoInaccessibleLocations

    // The maze is guarenteed to be nonempty and rectangular
    int width = maze.size();
    int height = maze.at(0).size();

    // Helper lambda for retrieving and adjacent tile if one exists, nullptr if not
    // TODO: MACK - this should be in maze utilities too
    auto getNeighbor = [&maze, &width, &height](int x, int y, Direction direction) {
        switch (direction) {
            case Direction::NORTH:
                return (y < height - 1 ? &maze[x][y + 1] : nullptr);
            case Direction::EAST:
                return (x < width - 1 ? &maze[x + 1][y] : nullptr);
            case Direction::SOUTH:
                return (0 < y ? &maze[x][y - 1] : nullptr);
            case Direction::WEST:
                return (0 < x ? &maze[x - 1][y] : nullptr);
        }
    };

    // Determine all of the center tiles
    // TODO: MACK - use the maze checker function for this
    QVector<Tile*> centerTiles;
            centerTiles.push_back(&maze[(width - 1) / 2][(height - 1) / 2]);
    if (width % 2 == 0) {
            centerTiles.push_back(&maze[ width      / 2][(height - 1) / 2]);
        if (height % 2 == 0) {
            centerTiles.push_back(&maze[(width - 1) / 2][ height      / 2]);
            centerTiles.push_back(&maze[ width      / 2][ height      / 2]);
        }
    }
    else if (height % 2 == 0) {
            centerTiles.push_back(&maze[(width - 1) / 2][ height      / 2]);
    }

    // The queue for the BFS
    QQueue<Tile*> discovered;

    // Set the distances of the center tiles and push them to the queue
    for (Tile* tile : centerTiles) {
        tile->setDistance(0); 
        discovered.enqueue(tile);
    }

    // Now do a BFS
    while (!discovered.empty()){
        Tile* tile = discovered.dequeue();
        for (Direction direction : DIRECTIONS()) {
            if (!tile->isWall(direction)) {
                Tile* neighbor = getNeighbor(tile->getX(), tile->getY(), direction);
                if (neighbor != nullptr && neighbor->getDistance() == -1) {
                    neighbor->setDistance(tile->getDistance() + 1);
                    discovered.enqueue(neighbor);
                }
            }
        }
    }

    return maze;
}

} // namespace mms
