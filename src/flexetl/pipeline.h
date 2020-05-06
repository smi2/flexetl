#include "pipebase.h"

class PipeLine : public PipeBase
{

public:
    virtual bool doProc(const std::filesystem::path &fileName)override;
};
