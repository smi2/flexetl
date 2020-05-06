#include "pipebase.h"

bool PipeBase::init(INode *pnode)
{
    m_pnode = pnode;

    return true;
}
bool PipeBase::start()
{
    std::string watchDir;
    if (!m_pnode->getStrParam("watch", watchDir))
    {
        m_pnode->logger()->error("PipeBase: No parametr 'watch'");
        return false;
    }

    if (!watchDir.empty() && !std::filesystem::exists(watchDir))
    {
        std::filesystem::create_directories(watchDir);
    }

    // перебираем папку на предмет существующих файлов
    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr(watchDir); itr != end_itr; itr++)
    {
        if (!is_directory(itr->status()))
        {
            doProc(itr->path());
        }
    }

    m_pnode->watch(watchDir, std::string());
    return true;
}
