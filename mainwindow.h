#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "GameModel.h"
#include <QTime>
#include <QtNetwork>
#include <QThread>

enum CStype
{
    CLIENT,
    SERVER
};
struct CSStruct
{
    int socketType;
    QTcpServer *server;
    QTcpSocket *connection;
};

class MainWindow;
class TimeUpdater : public QThread
{
    Q_OBJECT
public:
    TimeUpdater(MainWindow *mp);
    int timeLeft(){ return time_left; }
    QTime getLastTime() { return last_time; }
    void Reset();
protected:
    void run() override;
private:
    static const int kTotalTime = 10;
    int time_left;
    int delta;
    QTime last_time;
    bool reset_lock = false;
    MainWindow *parent_window;
signals:
    void updateTime(bool);
    void timeIsUp(int);
//private slots:
//    void updateTimeSlot();
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();
    struct CSStruct socket;
    GameType getGameType() { return game->gameType;}

protected:
    // 绘制
    void paintEvent(QPaintEvent *event);
    // 监听鼠标移动情况，方便落子
    void mouseMoveEvent(QMouseEvent *event);
    // 实际落子
    void mouseReleaseEvent(QMouseEvent *event);

private:
    GameModel *game; // 游戏指针
    GameType game_type; // 存储游戏类型
    TimeUpdater *timer;
    int clickPosRow, clickPosCol; // 存储将点击的位置
    void initGame();
    void checkGame(int y, int x);
    void checkTime();

private slots:
    void chessOneByPerson(); // 人执行
    void chessOneByAI(); // AI下棋
    void chessOneOL();
    void chessOne(int);

    void initPVPGame();
    void initPVEGame();
    void initPVPGameOL();

    void sendMessage(QString);
    void acceptConnection();
    void receiveData();
    void showError(QAbstractSocket::SocketError);
};

#endif // MAINWINDOW_H
