#include <QPainter>
#include <QTimer>
// #include <QSound>
#include <QMouseEvent>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <math.h>
#include "mainwindow.h"

#include <QLineEdit>
#include <QInputDialog>


// -------全局变量-------//
// #define CHESS_ONE_SOUND ":/res/sound/chessone.wav"
// #define WIN_SOUND ":/res/sound/win.wav"
// #define LOSE_SOUND ":/res/sound/lose.wav"

const int kBoardMargin = 50; // 棋盘边缘空隙
const int kBarWidth = 200;
const int kBlackRadius = 20; // 棋子半径
const int kWhiteRadius = 30;
const int kMarkSize = 6; // 落子标记边长
const int kBoardMarkRadius = 4;
const int kBlockSize = 40; // 格子的大小
const int kPosDelta = 20; // 鼠标点击的模糊距离上限

const int kAIDelay = 700; // AI下棋的思考时间

// -------------------- //

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 设置棋盘大小
    setFixedSize(kBoardMargin * 2 + kBlockSize * kBoardSizeNum + kBarWidth, kBoardMargin * 2 + kBlockSize * kBoardSizeNum);
//    setStyleSheet("background-color:yellow;");

    // 开启鼠标hover功能，这两句一般要设置window的
    setMouseTracking(true);
//    centralWidget()->setMouseTracking(true);

    // 添加菜单
    QMenu *gameMenu = menuBar()->addMenu(tr("Mode")); // menuBar默认是存在的，直接加菜单就可以了
    QAction *actionPVP = new QAction("Person VS Person", this);
    connect(actionPVP, SIGNAL(triggered()), this, SLOT(initPVPGame()));
    gameMenu->addAction(actionPVP);

    QAction *actionPVE = new QAction("Person VS Computer", this);
    connect(actionPVE, SIGNAL(triggered()), this, SLOT(initPVEGame()));
    gameMenu->addAction(actionPVE);

    QAction *actionPVPOL = new QAction("Person VS Person OL", this);
    connect(actionPVPOL, SIGNAL(triggered()), this, SLOT(initPVPGameOL()));
    gameMenu->addAction(actionPVPOL);

    timer = new TimeUpdater(this);
    // 开始游戏
    initGame();
}

MainWindow::~MainWindow()
{
//    qDebug() << "mainWindow destructed";
    if (game)
    {
        delete game;
        game = nullptr;
    }
}

void MainWindow::initGame()
{
    // 初始化游戏模型
    game = new GameModel;
    connect(timer, SIGNAL(updateTime(bool)), this, SLOT(update()), Qt::QueuedConnection);
    connect(timer, SIGNAL(timeIsUp(int)), this, SLOT(chessOne(int)), Qt::QueuedConnection);
    timer->start();
    initPVPGame();
}
void MainWindow::chessOne(int a)
{
    if (a == 1)
        chessOneByAI();
    else if (a == 2)
        chessOneOL();
}


void MainWindow::initPVPGame()
{
    game_type = PERSON;
    game->gameStatus = PLAYING;
    game->startGame(game_type);
    timer->Reset();
    update();
}

void MainWindow::initPVEGame()
{
    game_type = BOT;
    game->gameStatus = PLAYING;
    game->startGame(game_type);
    timer->Reset();
    update();
}

void MainWindow::initPVPGameOL()
{
    game_type=PVPOL;

    QString dlgTitle="加入房间";
    QString txtLabel="请输入服务器IP（按Cancel自动创建）";
    QString defaultInput="127.0.0.1";
    QLineEdit::EchoMode echoMode=QLineEdit::Normal;//正常文字输入
    bool ok=false;
    QString text = QInputDialog::getText(this, dlgTitle,txtLabel, echoMode,defaultInput, &ok);
    if (ok && !text.isEmpty())
    {
        socket.socketType = CStype::CLIENT;
        socket.connection = new QTcpSocket(this);
        socket.connection->abort();
        socket.connection->connectToHost(QHostAddress(text), 6665);
        connect(socket.connection, SIGNAL(readyRead()), this, SLOT(receiveData()));
        qDebug("[conn] client connected with %s", text.toStdString().data());
        game->cur = false;
    }
    else if (text.isEmpty())
    {
        qDebug("[init] room created as computer IP");
        socket.socketType = CStype::SERVER;
        socket.server = new QTcpServer();
        socket.connection = new QTcpSocket();
        if (!socket.server->listen(QHostAddress::Any, 6665))
        {
            qDebug() << socket.server->errorString();
            socket.server->close();
        }
        qDebug() << "[init] server listening";
        connect(socket.server, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
        connect(socket.connection, SIGNAL(error(QAbstractSocket::SocketError)),SLOT(showError(QAbstractSocket::SocketError)));
        game->cur = true;
    }
//    qDebug("connection test finished");
    game->gameStatus = PLAYING;
    game->startGame(game_type);
    timer->Reset();
    update();
}

void MainWindow::sendMessage(QString msg)
{
    socket.connection->write(msg.toStdString().data());
}
void MainWindow::acceptConnection()
{
    socket.connection = socket.server->nextPendingConnection();
    connect(socket.connection, SIGNAL(readyRead()), this, SLOT(receiveData()));
    qDebug() << "[conn] server connected";
    socket.server->close();
}
void MainWindow::receiveData()
{
    QString msg = socket.connection->readAll();
    qDebug(msg.toStdString().data());
    if (msg.mid(0,5) == "[pos]")
    {
//        qDebug() << "He prepares to commit";
        int rowOL = int(msg[5].toLatin1()-'A'), colOL = int(msg[6].toLatin1()-'A');
        game->actionByPerson(rowOL, colOL);
        update();
        qDebug() << "[recv] " << rowOL << ' ' << colOL;
        timer->Reset();
    }
}
void MainWindow::showError(QAbstractSocket::SocketError)
{
    qDebug()<<socket.server->errorString();
    socket.connection->close();
}
TimeUpdater::TimeUpdater(MainWindow *mp):
    time_left(kTotalTime),
    last_time(QTime::currentTime()),
    parent_window(mp) //给出一个timeisup的统一接口时的失败尝试就是因为注释掉了这一句
{ }

void TimeUpdater::run()
{
    while (true) {
        int newTmp = last_time.secsTo(QTime::currentTime());
        if (delta < newTmp)
        {
            delta = newTmp;
            time_left = kTotalTime - delta;
            emit updateTime(true);
        }
        if (time_left <= 0)
        {
            Reset();
            if (parent_window->getGameType() != PVPOL)
                emit timeIsUp(1);
            else emit timeIsUp(2);
        }
    }
}
void TimeUpdater::Reset()
{
//    reset_lock = true;
    last_time = QTime::currentTime();
    delta = 0;
    time_left = kTotalTime;
    emit updateTime(true);
//    reset_lock = false;
// there is no need for lock, just reset currentTime first
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    QPainter *painter = new QPainter();
    painter->begin(this);
    // 绘制棋盘
    painter->setRenderHint(QPainter::Antialiasing, true); // 抗锯齿
    painter->device();
    QPixmap pix;
    pix.load(":/res/board1.jpg");
    painter->drawPixmap(kBoardMargin-20, kBoardMargin-20, kBlockSize * kBoardSizeNum+20*2, kBlockSize * kBoardSizeNum+20*2, pix);

    QPen pen;
    pen.setWidthF(1.5);
    painter->setPen(pen);
    for (int i = 0; i < kBoardSizeNum + 1; i++)
    {
        painter->drawLine(kBoardMargin + kBlockSize * i, kBoardMargin, kBoardMargin + kBlockSize * i, kBlockSize * kBoardSizeNum + kBoardMargin);
        painter->drawLine(kBoardMargin, kBoardMargin + kBlockSize * i, kBlockSize * kBoardSizeNum + kBoardMargin, kBoardMargin + kBlockSize * i);
    }

    QBrush brush;
    //绘制棋盘参照点
    brush.setStyle(Qt::SolidPattern);
    brush.setColor(Qt::black);
    painter->setBrush(brush);
    painter->drawEllipse(kBoardMargin + kBlockSize * (kBoardSizeNum/4) - kBoardMarkRadius, kBoardMargin + kBlockSize * (kBoardSizeNum/4) - kBoardMarkRadius, kBoardMarkRadius * 2, kBoardMarkRadius * 2);
    painter->drawEllipse(kBoardMargin + kBlockSize * (kBoardSizeNum/4) - kBoardMarkRadius, kBoardMargin + kBlockSize * (kBoardSizeNum-kBoardSizeNum/4) - kBoardMarkRadius, kBoardMarkRadius * 2, kBoardMarkRadius * 2);
    painter->drawEllipse(kBoardMargin + kBlockSize * (kBoardSizeNum-kBoardSizeNum/4) - kBoardMarkRadius, kBoardMargin + kBlockSize * (kBoardSizeNum/4) - kBoardMarkRadius, kBoardMarkRadius * 2, kBoardMarkRadius * 2);
    painter->drawEllipse(kBoardMargin + kBlockSize * (kBoardSizeNum-kBoardSizeNum/4) - kBoardMarkRadius, kBoardMargin + kBlockSize * (kBoardSizeNum-kBoardSizeNum/4) - kBoardMarkRadius, kBoardMarkRadius * 2, kBoardMarkRadius * 2);
    painter->drawEllipse(kBoardMargin + kBlockSize * (kBoardSizeNum/2) - kBoardMarkRadius, kBoardMargin + kBlockSize * (kBoardSizeNum/2) - kBoardMarkRadius, kBoardMarkRadius * 2, kBoardMarkRadius * 2);

    //绘制状态显示菜单
    QFont titlefont("微软雅黑",30,QFont::Bold);
    painter->setFont(titlefont);
    painter->drawText(size().width()-175, 100, "模式");
    painter->drawText(size().width()-190, 250, "落子方");
    painter->drawText(size().width()-200, 400, "剩余时间");
    QFont contFont("宋体",25);
    painter->setFont(contFont);
    QString modeStr;
    if (game->gameType == PERSON)
        modeStr = "双人对战";
    else if (game->gameType == BOT)
        modeStr = "人机对战";
    else if (game->gameType == PVPOL)
        modeStr = "联机对战";
    painter->drawText(size().width()-190, 170, modeStr);
    if (game->playerFlag)
    {
        pix.load(":/res/black.png");
        painter->drawPixmap(size().width()-170, 270, 80,80,pix);
    }
    else
    {
        pix.load(":/res/white.png");
        painter->drawPixmap(size().width()-185, 250, 110,110,pix);
    }
    painter->drawText(size().width()-150, 440, QString::number(timer->timeLeft()));


    // 绘制落子标记(防止鼠标出框越界)
    if (clickPosRow > 0 && clickPosRow < kBoardSizeNum &&
        clickPosCol > 0 && clickPosCol < kBoardSizeNum &&
        game->gameMapVec[clickPosRow][clickPosCol] == 0)
    {
        if (game->playerFlag)
            brush.setColor(Qt::white);
        else
            brush.setColor(Qt::black);
        painter->setBrush(brush);
        painter->drawRect(kBoardMargin + kBlockSize * clickPosCol - kMarkSize / 2, kBoardMargin + kBlockSize * clickPosRow - kMarkSize / 2, kMarkSize, kMarkSize);
    }

    // 绘制棋子
    for (int i = 0; i < kBoardSizeNum; i++)
        for (int j = 0; j < kBoardSizeNum; j++)
        {
            if (game->gameMapVec[i][j] == 1)
            {
                QPixmap tmp;
                tmp.load(":/res/white.png");
                painter->drawPixmap(kBoardMargin + kBlockSize * j - kWhiteRadius, kBoardMargin + kBlockSize * i - kWhiteRadius, kWhiteRadius * 2, kWhiteRadius * 2, tmp);
//                brush.setColor(Qt::white);
//                painter->setBrush(brush);
//                painter->drawEllipse(kBoardMargin + kBlockSize * j - kRadius, kBoardMargin + kBlockSize * i - kRadius, kRadius * 2, kRadius * 2);
            }
            else if (game->gameMapVec[i][j] == -1)
            {
                QPixmap tmp;
                tmp.load(":/res/black.png");
                painter->drawPixmap(kBoardMargin + kBlockSize * j - kBlackRadius, kBoardMargin + kBlockSize * i - kBlackRadius, kBlackRadius * 2, kBlackRadius * 2, tmp);
//                brush.setColor(Qt::black);
//                painter->setBrush(brush);
//                painter->drawEllipse(kBoardMargin + kBlockSize * j - kRadius, kBoardMargin + kBlockSize * i - kRadius, kRadius * 2, kRadius * 2);
            }
        }

    // 判断输赢
    if (clickPosRow > 0 && clickPosRow < kBoardSizeNum &&
        clickPosCol > 0 && clickPosCol < kBoardSizeNum &&
        (game->gameMapVec[clickPosRow][clickPosCol] == 1 ||
            game->gameMapVec[clickPosRow][clickPosCol] == -1))
    {
        if (game->isWin(clickPosRow, clickPosCol) && game->gameStatus == PLAYING)
        {
            qDebug() << "win";
            game->gameStatus = WIN;
            // QSound::play(WIN_SOUND);
            QString str;
            if (game->gameMapVec[clickPosRow][clickPosCol] == 1)
                str = "white player";
            else if (game->gameMapVec[clickPosRow][clickPosCol] == -1)
                str = "black player";
            QMessageBox::StandardButton btnValue = QMessageBox::information(this, "congratulations", str + " win!");

            // 重置游戏状态，否则容易死循环
            if (btnValue == QMessageBox::Ok)
            {
                game->startGame(game_type);
                game->gameStatus = PLAYING;
            }

        }
    }


    // 判断死局
    if (game->isDeadGame())
    {
        // QSound::play(LOSE_SOUND);
        QMessageBox::StandardButton btnValue = QMessageBox::information(this, "oops", "dead game!");
        if (btnValue == QMessageBox::Ok)
        {
            game->startGame(game_type);
            game->gameStatus = PLAYING;
        }

    }
    painter->end();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // 通过鼠标的hover确定落子的标记
    int x = event->x();
    int y = event->y();

    // 棋盘边缘不能落子
    if (x >= kBoardMargin + kBlockSize / 2 &&
            x < kBlockSize * kBoardSizeNum + kBoardMargin - kBlockSize / 2 &&
            y >= kBoardMargin + kBlockSize / 2 &&
            y < kBlockSize * kBoardSizeNum + kBoardMargin - kBlockSize / 2)
    {
        // 获取最近的左上角的点
        int col = (x-kBoardMargin) / kBlockSize;
        int row = (y-kBoardMargin) / kBlockSize;
//qDebug() << col << row;
        int leftTopPosX = kBoardMargin + kBlockSize * col;
        int leftTopPosY = kBoardMargin + kBlockSize * row;

        // 根据距离算出合适的点击位置,一共四个点，根据半径距离选最近的
        clickPosRow = -1; // 初始化最终的值
        clickPosCol = -1;
        int len = 0; // 计算完后取整就可以了

        // 确定一个误差在范围内的点，且只可能确定一个出来
        len = sqrt((x - leftTopPosX) * (x - leftTopPosX) + (y - leftTopPosY) * (y - leftTopPosY));
        if (len < kPosDelta)
        {
            clickPosRow = row;
            clickPosCol = col;
        }
        len = sqrt((x - leftTopPosX - kBlockSize) * (x - leftTopPosX - kBlockSize) + (y - leftTopPosY) * (y - leftTopPosY));
        if (len < kPosDelta)
        {
            clickPosRow = row;
            clickPosCol = col + 1;
        }
        len = sqrt((x - leftTopPosX) * (x - leftTopPosX) + (y - leftTopPosY - kBlockSize) * (y - leftTopPosY - kBlockSize));
        if (len < kPosDelta)
        {
            clickPosRow = row + 1;
            clickPosCol = col;
        }
        len = sqrt((x - leftTopPosX - kBlockSize) * (x - leftTopPosX - kBlockSize) + (y - leftTopPosY - kBlockSize) * (y - leftTopPosY - kBlockSize));
        if (len < kPosDelta)
        {
            clickPosRow = row + 1;
            clickPosCol = col + 1;
        }
//qDebug() << clickPosRow << clickPosCol;
    }

    // 存了坐标后也要重绘
    update();
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (game_type == PERSON)
        chessOneByPerson();
    // 人下棋，并且不能抢机器的棋
    if (game_type == BOT)
    {
        chessOneByPerson();
        QTimer::singleShot(kAIDelay, this, SLOT(chessOneByAI()));
    }
    if (game_type == PVPOL)
        chessOneOL();
}

void MainWindow::chessOneByPerson()
{
    // 根据当前存储的坐标下子
    // 只有有效点击才下子，并且该处没有子
    if (clickPosRow != -1 && clickPosCol != -1 && game->gameMapVec[clickPosRow][clickPosCol] == 0)
    {
        game->actionByPerson(clickPosRow, clickPosCol);
        // QSound::play(CHESS_ONE_SOUND);
        timer->Reset();
        // 重绘
        update();
    }
}

void MainWindow::chessOneByAI()
{
    game->actionByAI(clickPosRow, clickPosCol);
    // QSound::play(CHESS_ONE_SOUND);
    timer->Reset();
    update();
}

void MainWindow::chessOneOL()
{
//    qDebug() << "I prepare to commit:" << game->cur;
    if (game->playerFlag == game->cur)
    {
        if (timer->timeLeft() <= 0)
            game->calculateByAI(clickPosRow, clickPosCol);
        if (clickPosCol != -1 && clickPosRow != -1 && game->gameMapVec[clickPosRow][clickPosCol] == 0)
        {
            qDebug() << "[send] " << clickPosRow << ' ' <<clickPosCol;
            game->actionByPerson(clickPosRow,clickPosCol);
            update();
            QString Pos = "[pos]";
            Pos += char('A'+clickPosRow);
            Pos += char('A'+clickPosCol);
            sendMessage(Pos);
            timer->Reset();
        }
        else qDebug() << "[error] wrong place";
    }
    else qDebug() << "[error] not your turn";
}

