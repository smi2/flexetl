#pragma once

#include "handler.h"

class PipeBase : public IHandler
{
protected:
    INode *m_pnode = 0;

    virtual bool init(INode *pnode)override;
    virtual bool start()override;
};
