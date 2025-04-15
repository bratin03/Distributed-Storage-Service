#ifndef THREE_WAY_MERGE_HPP
#define THREE_WAY_MERGE_HPP

#include <cstring>
#include <git2.h>
#include <string>

namespace MergeLib {

/*
 * three_way_merge
 *
 * Parameters:
 *   base   - The common ancestor version (base) of the file.
 *   local  - The "ours" version (local changes).
 *   remote - The "theirs" version (remote changes).
 *   final  - (Output) On success, will contain the merged result.
 *
 * Returns:
 *   true if the merge was performed automatically without conflicts;
 *   false if a conflict was detected (in which case final is empty).
 *
 * This function uses libgit2's git_merge_file API.
 */
inline bool three_way_merge(const std::string &base, const std::string &local,
                            const std::string &remote, std::string &final) {
  // Initialize libgit2. In a larger application, you might do this once at
  // startup.
  git_libgit2_init();

  // Prepare the input structures for the merge.
  git_merge_file_input ancestor = {0};
  git_merge_file_input ours = {0};
  git_merge_file_input theirs = {0};

  ancestor.ptr = base.c_str();
  ancestor.size = base.size();
  ancestor.mode = 0100644; // typical file mode for text files

  ours.ptr = local.c_str();
  ours.size = local.size();
  ours.mode = 0100644;

  theirs.ptr = remote.c_str();
  theirs.size = remote.size();
  theirs.mode = 0100644;

  // Use the default merge options.
  git_merge_file_options opts = GIT_MERGE_FILE_OPTIONS_INIT;

  // Prepare a structure to hold the merge result.
  git_merge_file_result result;
  std::memset(&result, 0, sizeof(result));

  // Perform the three-way merge.
  int error = git_merge_file(&result, &ancestor, &ours, &theirs, &opts);

  if (error != 0 || result.ptr == nullptr) {
    final.clear();
    git_libgit2_shutdown();
    return false;
  }

  // Check if merge is automergeable.
  bool success = (result.automergeable != 0);
  if (success) {
    final.assign(result.ptr, result.len);
  } else {
    final.clear();
  }

  // Free the merge result memory; cast away const to satisfy free().
  free(const_cast<void *>(static_cast<const void *>(result.ptr)));

  // Shutdown libgit2.
  git_libgit2_shutdown();

  return success;
}

} // namespace MergeLib

#endif // THREE_WAY_MERGE_HPP
