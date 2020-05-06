#include "pipeset.h"

bool PipeSet::doProc(const std::filesystem::path &fileName)
{
    bool res = true;
    for (int i = 0; i < m_pnode->subCount(); i++)
    {
        auto hundler = m_pnode->subHundler(i);
        if (hundler)
            res = res && hundler->doProc(fileName);
    }
    m_pnode->moveFile("", fileName);

    return res;
}
