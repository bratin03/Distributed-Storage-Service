#include "merge.hpp"
#include <git2.h>
#include <cstring>
#include "../logger/Mylogger.hpp"

namespace merge
{
    bool mergeCheck(const std::string &base, const std::string &local,
                    const std::string &remote, std::string &final)
    {
        git_libgit2_init();

        MyLogger::info("Merge >> Base: " + base);
        MyLogger::info("Merge >> Local: " + local);
        MyLogger::info("Merge >> Remote: " + remote);

        // Prepare inputs
        git_merge_file_input ancestor = GIT_MERGE_FILE_INPUT_INIT;
        git_merge_file_input ours = GIT_MERGE_FILE_INPUT_INIT;
        git_merge_file_input theirs = GIT_MERGE_FILE_INPUT_INIT;

        ancestor.ptr = base.c_str();
        ancestor.size = base.size();
        ancestor.mode = 0100644;

        ours.ptr = local.c_str();
        ours.size = local.size();
        ours.mode = 0100644;

        theirs.ptr = remote.c_str();
        theirs.size = remote.size();
        theirs.mode = 0100644;

        // Merge options
        git_merge_file_options opts = GIT_MERGE_FILE_OPTIONS_INIT;
        opts.flags = GIT_MERGE_FILE_IGNORE_WHITESPACE |
                     GIT_MERGE_FILE_IGNORE_WHITESPACE_CHANGE |
                     GIT_MERGE_FILE_IGNORE_WHITESPACE_EOL |
                     GIT_MERGE_FILE_DIFF_PATIENCE |
                     GIT_MERGE_FILE_DIFF_MINIMAL |
                     GIT_MERGE_FILE_SIMPLIFY_ALNUM;

        git_merge_file_result result;
        std::memset(&result, 0, sizeof(result));

        int error = git_merge_file(&result, &ancestor, &ours, &theirs, &opts);
        if (error != 0 || result.ptr == nullptr)
        {
            final.clear();
            git_libgit2_shutdown();
            MyLogger::warning("Merge >> Failed due to error: " + std::to_string(error));
            return false;
        }

        bool success = (result.automergeable != 0);
        if (success)
        {
            final.assign(result.ptr, result.len);
            MyLogger::info("Merge >> Success: " + final);
        }
        else
        {
            final.clear();
            MyLogger::warning("Merge >> Not automergeable");
        }

        git_merge_file_result_free(&result);
        git_libgit2_shutdown();

        return success;
    }
} // namespace merge
