#pragma once

#include "WorkerThread.h"
#include "ExplorerModel.h"
#include "Settings.h"
#include <string>
#include <vector>
#include <memory>

class TaskInit : public IAsyncTask {
public:
    TaskInit(std::shared_ptr<ExplorerModel> model, Settings* settings);

    void Execute() override;
    void OnCompleted() override;

private:
    std::shared_ptr<ExplorerModel> _model;
    Settings* _settings;
    std::shared_ptr<ExplorerEntry> _root;
};

class TaskUpdateDirectory : public IAsyncTask {
public:
    TaskUpdateDirectory(std::shared_ptr<ExplorerModel> model, std::shared_ptr<ExplorerEntry> entry, Settings* settings);

    void Execute() override;
    void OnCompleted() override;

private:
    std::shared_ptr<ExplorerModel> _model;
    std::shared_ptr<ExplorerEntry> _entry;
    Settings* _settings;
    std::vector<std::shared_ptr<ExplorerEntry>> _children;
};
