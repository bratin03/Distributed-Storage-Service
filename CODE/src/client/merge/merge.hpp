/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#ifndef MERGE_HPP
#define MERGE_HPP

#include <string>

namespace merge {
/**
 * @brief Perform a three-way merge using libgit2 with tuned diff flags.
 *
 * @param base   The base (common ancestor) version of the file.
 * @param local  The "ours" version with local changes.
 * @param remote The "theirs" version with incoming/remote changes.
 * @param final  (Output) If merge succeeds, contains the merged result.
 *
 * @return true  if the merge was completed automatically without conflicts.
 * @return false if a conflict was detected or an error occurred.
 *
 * @note Internally initializes and shuts down libgit2 for each call.
 */
bool mergeCheck(const std::string &base, const std::string &local, const std::string &remote,
                std::string &final);
}  // namespace merge

#endif  // MERGE_HPP
