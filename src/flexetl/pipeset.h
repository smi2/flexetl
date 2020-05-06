#include "pipebase.h"

class PipeSet : public PipeBase
{
public:
    virtual bool doProc(const std::filesystem::path &fileName)override;
};
