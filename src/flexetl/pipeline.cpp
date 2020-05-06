#include "pipeline.h"

bool PipeLine::doProc(const std::filesystem::path &fileName)
{

    for (int i = 0; i < m_pnode->subCount(); i++)
    {
        auto hundler = m_pnode->subHundler(i);
        if (!hundler)
            return false;
        bool res = hundler->doProc(fileName);
        if (!res)
            return false;
    }

    return PipeBase::doProc(fileName);
}
