#pragma once
#include <QApplication>
#include <memory>

class Application : public QApplication {
public:
    Application(int &argc, char *argv[]);
    ~Application();

    bool init();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
