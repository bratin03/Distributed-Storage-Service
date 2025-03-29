#include <iostream>
#include <string>
#include "three_way_merge.hpp" // Include your header with the three_way_merge function

using namespace std;

int main() {
    // -----------------------------
    // Example 1: Mergeable Changes
    // -----------------------------
    // Base file: a common starting point.
    string base1 = "Line 1: Common text.\nLine 2: Common text.\n";
    // Both local and remote have made the same change to line 2.
    string local1 = "Line 1: Common text.\nLine 2: Modified text.\n";
    string remote1 = "Line 1: Common text.\nLine 2: Modified text.\n";
    string merged1;

    cout << "=== Mergeable Example ===" << endl;
    if (MergeLib::three_way_merge(base1, local1, remote1, merged1)) {
        cout << "Merge succeeded. Merged result:" << endl;
        cout << merged1 << endl;
    } else {
        cout << "Merge failed due to conflicts." << endl;
    }

    // ---------------------------------
    // Example 2: Unmergeable (Conflict)
    // ---------------------------------
    // Base file: a common starting point.
    string base2 = "Line 1: Common text.\nLine 2: Common text.\n";
    // Local and remote each modify line 2 differently.
    string local2 = "Line 1: Common text.\nLine 2: Local modification.\n";
    string remote2 = "Line 1: Common text.\nLine 2: Remote modification.\n";
    string merged2;

    cout << "\n=== Unmergeable Example ===" << endl;
    if (MergeLib::three_way_merge(base2, local2, remote2, merged2)) {
        cout << "Merge succeeded. Merged result:" << endl;
        cout << merged2 << endl;
    } else {
        cout << "Merge failed due to conflicts." << endl;
    }

    return 0;
}
