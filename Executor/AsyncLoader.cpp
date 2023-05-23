#include "AsyncLoader.hpp"
#include "AsyncLoaderMgr.hpp"

AsyncLoader::AsyncLoader(const std::filesystem::path &source, LoadPriority priority) :
   source(source), priority(priority)
{
}

AsyncLoader::~AsyncLoader()
{
}
