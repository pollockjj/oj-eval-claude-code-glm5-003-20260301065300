#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>

using namespace std;

enum class Status {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

Status parseStatus(const string& s) {
    if (s == "Accepted") return Status::Accepted;
    if (s == "Wrong_Answer") return Status::Wrong_Answer;
    if (s == "Runtime_Error") return Status::Runtime_Error;
    return Status::Time_Limit_Exceed;
}

string statusToString(Status s) {
    switch (s) {
        case Status::Accepted: return "Accepted";
        case Status::Wrong_Answer: return "Wrong_Answer";
        case Status::Runtime_Error: return "Runtime_Error";
        case Status::Time_Limit_Exceed: return "Time_Limit_Exceed";
    }
    return "";
}

struct Submission {
    char problem;
    Status status;
    int time;
};

struct ProblemState {
    int wrongBeforeFreeze = 0;  // Wrong attempts before freeze
    int submitAfterFreeze = 0;  // Submissions after freeze
    int wrongTotal = 0;         // Total wrong attempts
    bool solved = false;
    int solveTime = 0;
    bool solvedBeforeFreeze = false;  // Was it solved before freeze?
    int wrongBeforeSolve = 0;  // Wrong attempts before first solve
};

struct Team {
    string name;
    map<char, ProblemState> problems;
    vector<Submission> submissions;
};

class ICPCSystem {
private:
    map<string, Team> teams;
    bool started = false;
    int duration = 0;
    int problemCount = 0;
    bool frozen = false;
    bool flushed = false;
    vector<string> teamOrder;  // Order after last flush

    // Get solved count (optionally excluding currently frozen problems)
    int getSolvedCount(const Team& team, bool excludeFrozen = false) const {
        int count = 0;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end() && it->second.solved) {
                // Exclude if: we want to exclude frozen, AND this problem is currently frozen
                bool isFrozen = !it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0;
                if (!excludeFrozen || !isFrozen) {
                    count++;
                }
            }
        }
        return count;
    }

    // Get penalty time (optionally excluding currently frozen problems)
    int getPenaltyTime(const Team& team, bool excludeFrozen = false) const {
        int penalty = 0;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end() && it->second.solved) {
                bool isFrozen = !it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0;
                if (!excludeFrozen || !isFrozen) {
                    penalty += 20 * it->second.wrongBeforeSolve + it->second.solveTime;
                }
            }
        }
        return penalty;
    }

    // Get solve times sorted descending (optionally excluding currently frozen)
    vector<int> getSolveTimes(const Team& team, bool excludeFrozen = false) const {
        vector<int> times;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end() && it->second.solved) {
                bool isFrozen = !it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0;
                if (!excludeFrozen || !isFrozen) {
                    times.push_back(it->second.solveTime);
                }
            }
        }
        sort(times.begin(), times.end(), greater<int>());
        return times;
    }

    // Compare teams for ranking (returns true if a ranks higher than b)
    bool compareTeams(const string& a, const string& b, bool excludeFrozen = false) const {
        auto itA = teams.find(a);
        auto itB = teams.find(b);
        const Team& teamA = itA->second;
        const Team& teamB = itB->second;

        int solvedA = getSolvedCount(teamA, excludeFrozen);
        int solvedB = getSolvedCount(teamB, excludeFrozen);
        if (solvedA != solvedB) return solvedA > solvedB;

        int penaltyA = getPenaltyTime(teamA, excludeFrozen);
        int penaltyB = getPenaltyTime(teamB, excludeFrozen);
        if (penaltyA != penaltyB) return penaltyA < penaltyB;

        vector<int> timesA = getSolveTimes(teamA, excludeFrozen);
        vector<int> timesB = getSolveTimes(teamB, excludeFrozen);

        for (size_t i = 0; i < min(timesA.size(), timesB.size()); i++) {
            if (timesA[i] != timesB[i]) return timesA[i] < timesB[i];
        }

        return a < b;
    }

    // Get current ranking order
    vector<string> getRankingOrder(bool excludeFrozen = false) const {
        vector<string> order;
        for (const auto& p : teams) {
            order.push_back(p.first);
        }
        sort(order.begin(), order.end(), [this, excludeFrozen](const string& a, const string& b) {
            return this->compareTeams(a, b, excludeFrozen);
        });
        return order;
    }

    void flushScoreboard() {
        teamOrder = getRankingOrder(true);  // Exclude frozen
        flushed = true;
    }

    string formatProblemStatus(const Team& team, char p, bool showFrozen = true) const {
        auto it = team.problems.find(p);
        if (it == team.problems.end()) {
            return ".";
        }

        const ProblemState& ps = it->second;

        // Check if this problem is currently frozen (and we're showing frozen state)
        bool isFrozen = showFrozen && !ps.solvedBeforeFreeze && ps.submitAfterFreeze > 0;

        if (isFrozen) {
            // Frozen: show wrong_before_freeze/submissions_after_freeze
            int wrong = ps.wrongBeforeFreeze;
            if (wrong == 0) {
                return "0/" + to_string(ps.submitAfterFreeze);
            } else {
                return "-" + to_string(wrong) + "/" + to_string(ps.submitAfterFreeze);
            }
        } else if (ps.solved) {
            // Solved
            int wrong = ps.wrongBeforeSolve;
            if (wrong == 0) {
                return "+";
            } else {
                return "+" + to_string(wrong);
            }
        } else {
            // Not solved
            int wrong = ps.wrongTotal;
            if (wrong == 0) {
                return ".";
            } else {
                return "-" + to_string(wrong);
            }
        }
    }

    void printTeam(const string& name, int rank, bool excludeFrozen = false, bool showFrozen = true) const {
        const Team& team = teams.at(name);
        int solved = getSolvedCount(team, excludeFrozen);
        int penalty = getPenaltyTime(team, excludeFrozen);

        cout << name << " " << rank << " " << solved << " " << penalty;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            cout << " " << formatProblemStatus(team, p, showFrozen);
        }
        cout << "\n";
    }

    void printScoreboard(bool excludeFrozen = false, bool showFrozen = true) const {
        vector<string> order = getRankingOrder(excludeFrozen);

        int rank = 1;
        for (size_t i = 0; i < order.size(); i++) {
            if (i > 0 && compareTeams(order[i-1], order[i], excludeFrozen)) {
                rank = i + 1;
            }
            printTeam(order[i], rank, excludeFrozen, showFrozen);
        }
    }

    int getTeamRankFromOrder(const string& name, const vector<string>& order, bool excludeFrozen) const {
        for (size_t i = 0; i < order.size(); i++) {
            if (order[i] == name) {
                // Calculate actual rank (handling ties)
                int rank = 1;
                for (size_t j = 0; j < i; j++) {
                    if (compareTeams(order[j], order[i], excludeFrozen)) {
                        rank = j + 2;
                    }
                }
                return rank;
            }
        }
        return -1;
    }

    int getStoredRank(const string& name) const {
        if (!flushed) {
            // Before first flush, use lexicographic order
            vector<string> order;
            for (const auto& p : teams) {
                order.push_back(p.first);
            }
            sort(order.begin(), order.end());
            for (size_t i = 0; i < order.size(); i++) {
                if (order[i] == name) return i + 1;
            }
            return -1;
        }
        return getTeamRankFromOrder(name, teamOrder, true);
    }

    // Check if a team has frozen problems
    bool hasFrozenProblems(const Team& team) const {
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end()) {
                if (!it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0) {
                    return true;
                }
            }
        }
        return false;
    }

    // Get smallest frozen problem for a team
    char getSmallestFrozenProblem(const Team& team) const {
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end()) {
                if (!it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0) {
                    return p;
                }
            }
        }
        return '\0';
    }

    // Unfreeze a problem for a team
    void unfreezeProblem(Team& team, char p) {
        auto it = team.problems.find(p);
        if (it != team.problems.end()) {
            ProblemState& ps = it->second;
            // Now we reveal the result: if there was an Accepted submission during freeze,
            // mark it as solved
            int acceptedTime = -1;
            int wrongAfterSolve = 0;
            bool solvedDuringFreeze = false;
            int wrongBeforeSolveDuringFreeze = ps.wrongBeforeFreeze;

            for (const auto& sub : team.submissions) {
                if (sub.problem == p) {
                    if (sub.status == Status::Accepted && acceptedTime == -1) {
                        acceptedTime = sub.time;
                        solvedDuringFreeze = true;
                    } else if (acceptedTime == -1) {
                        // Wrong submission before first accepted
                        if (!ps.solvedBeforeFreeze) {
                            wrongBeforeSolveDuringFreeze++;
                        }
                    }
                }
            }

            if (solvedDuringFreeze) {
                ps.solved = true;
                ps.solveTime = acceptedTime;
                ps.wrongBeforeSolve = wrongBeforeSolveDuringFreeze;
            }

            // Clear frozen state
            ps.submitAfterFreeze = 0;
        }
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // Print scoreboard before scrolling (excluding frozen problems from ranking, but show frozen status)
        printScoreboard(true, true);

        // Store ranking changes
        vector<tuple<string, string, int, int>> changes;

        // Get initial order (excluding frozen)
        vector<string> prevOrder = getRankingOrder(true);

        // Process each frozen problem
        while (true) {
            // Find lowest-ranked team with frozen problems
            vector<string> teamsWithFrozen;
            for (const auto& p : teams) {
                if (hasFrozenProblems(p.second)) {
                    teamsWithFrozen.push_back(p.first);
                }
            }

            if (teamsWithFrozen.empty()) break;

            // Sort by current ranking (lowest first, i.e., reverse order)
            sort(teamsWithFrozen.begin(), teamsWithFrozen.end(), [this](const string& a, const string& b) {
                return !this->compareTeams(a, b, false);  // Include all solved problems
            });

            // Get lowest-ranked team
            string teamName = teamsWithFrozen.back();
            char problem = getSmallestFrozenProblem(teams[teamName]);

            if (problem == '\0') break;

            // Get the team's rank before unfreezing (excluding frozen)
            int oldRank = getTeamRankFromOrder(teamName, prevOrder, true);

            // Unfreeze this problem
            unfreezeProblem(teams[teamName], problem);

            // Get new order
            vector<string> newOrder = getRankingOrder(false);

            // Get the team's new rank
            int newRank = getTeamRankFromOrder(teamName, newOrder, false);

            // Check if ranking changed
            if (newRank < oldRank) {
                // Ranking improved - find the team that was replaced
                string replacedTeam = prevOrder[newRank - 1];
                int newSolved = getSolvedCount(teams[teamName], false);
                int newPenalty = getPenaltyTime(teams[teamName], false);
                changes.push_back({teamName, replacedTeam, newSolved, newPenalty});
            }

            prevOrder = newOrder;
        }

        // Print ranking changes
        for (const auto& c : changes) {
            cout << get<0>(c) << " " << get<1>(c) << " " << get<2>(c) << " " << get<3>(c) << "\n";
        }

        // Print scoreboard after scrolling (no more frozen problems)
        printScoreboard(false, false);

        // Clear frozen state and flush
        frozen = false;
        flushScoreboard();
    }

public:
    void addTeam(const string& name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (teams.count(name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        teams[name] = Team{name};
        cout << "[Info]Add successfully.\n";
    }

    void startCompetition(int dur, int probs) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        duration = dur;
        problemCount = probs;
        started = true;
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& teamName, const string& status, int time) {
        if (!started) return;

        Status s = parseStatus(status);
        auto& team = teams[teamName];
        char p = problem[0];

        team.submissions.push_back({p, s, time});

        auto& ps = team.problems[p];

        if (frozen) {
            // Submission during freeze
            if (!ps.solvedBeforeFreeze) {
                // Problem not solved before freeze
                ps.submitAfterFreeze++;
                if (s == Status::Accepted) {
                    // Will be revealed during scroll
                } else {
                    // Wrong during freeze, still frozen
                }
            } else {
                // Already solved before freeze, just track but no effect on scoreboard
            }
        } else {
            // Normal submission
            if (s == Status::Accepted && !ps.solved) {
                ps.solved = true;
                ps.solveTime = time;
                ps.wrongBeforeSolve = ps.wrongTotal;
                ps.solvedBeforeFreeze = true;  // Mark as solved before any future freeze
            } else if (s != Status::Accepted && !ps.solved) {
                ps.wrongTotal++;
            }
        }
    }

    void flush() {
        flushScoreboard();
        cout << "[Info]Flush scoreboard.\n";
    }

    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        // Mark all problems with their state before freeze
        for (auto& p : teams) {
            for (auto& q : p.second.problems) {
                if (q.second.solved) {
                    q.second.solvedBeforeFreeze = true;
                }
                // Track wrong attempts before freeze for all problems (solved or not)
                q.second.wrongBeforeFreeze = q.second.wrongTotal;
            }
        }
        frozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    void performScroll() {
        scroll();
    }

    void queryRanking(const string& teamName) {
        if (!teams.count(teamName)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";

        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        int rank = getStoredRank(teamName);
        cout << teamName << " NOW AT RANKING " << rank << "\n";
    }

    void querySubmission(const string& teamName, const string& problem, const string& status) {
        if (!teams.count(teamName)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        const Team& team = teams.at(teamName);
        const Submission* lastMatch = nullptr;

        for (const auto& sub : team.submissions) {
            bool match = true;

            if (problem != "ALL") {
                if (string(1, sub.problem) != problem) match = false;
            }

            if (status != "ALL") {
                if (statusToString(sub.status) != status) match = false;
            }

            if (match) {
                lastMatch = &sub;
            }
        }

        if (!lastMatch) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << teamName << " " << lastMatch->problem << " "
                 << statusToString(lastMatch->status) << " " << lastMatch->time << "\n";
        }
    }

    void end() {
        cout << "[Info]Competition ends.\n";
    }
};

vector<string> split(const string& s) {
    vector<string> tokens;
    string token;
    istringstream iss(s);
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        vector<string> tokens = split(line);
        string cmd = tokens[0];

        if (cmd == "ADDTEAM") {
            system.addTeam(tokens[1]);
        } else if (cmd == "START") {
            int dur = stoi(tokens[2]);
            int probs = stoi(tokens[4]);
            system.startCompetition(dur, probs);
        } else if (cmd == "SUBMIT") {
            string problem = tokens[1];
            string teamName = tokens[3];
            string status = tokens[5];
            int time = stoi(tokens[7]);
            system.submit(problem, teamName, status, time);
        } else if (cmd == "FLUSH") {
            system.flush();
        } else if (cmd == "FREEZE") {
            system.freeze();
        } else if (cmd == "SCROLL") {
            system.performScroll();
        } else if (cmd == "QUERY_RANKING") {
            system.queryRanking(tokens[1]);
        } else if (cmd == "QUERY_SUBMISSION") {
            string teamName = tokens[1];
            string problem = tokens[3].substr(8);  // PROBLEM=xxx
            string status = tokens[5].substr(7);   // STATUS=xxx
            system.querySubmission(teamName, problem, status);
        } else if (cmd == "END") {
            system.end();
            break;
        }
    }

    return 0;
}
