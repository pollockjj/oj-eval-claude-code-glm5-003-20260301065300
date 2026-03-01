#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std;

// Status for a problem submission
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

// Submission record
struct Submission {
    char problem;
    Status status;
    int time;
    bool afterFreeze;
};

// Problem state for a team
struct ProblemState {
    int wrongAttempts = 0;      // Total wrong attempts
    int wrongBeforeFreeze = 0;  // Wrong attempts before freeze
    int submitAfterFreeze = 0;  // Submissions after freeze
    bool solved = false;
    int solveTime = 0;
    bool frozen = false;        // Is this problem in frozen state
};

// Team information
struct Team {
    string name;
    map<char, ProblemState> problems;
    vector<Submission> submissions;

    int getSolvedCount(int problemCount, bool includeFrozen = true) const {
        int count = 0;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = problems.find(p);
            if (it != problems.end() && it->second.solved) {
                if (includeFrozen || !it->second.frozen) {
                    count++;
                }
            }
        }
        return count;
    }

    int getTotalPenalty(int problemCount, bool includeFrozen = true) const {
        int penalty = 0;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = problems.find(p);
            if (it != problems.end() && it->second.solved) {
                if (includeFrozen || !it->second.frozen) {
                    penalty += 20 * it->second.wrongAttempts + it->second.solveTime;
                }
            }
        }
        return penalty;
    }

    vector<int> getSolveTimes(int problemCount, bool includeFrozen = true) const {
        vector<int> times;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = problems.find(p);
            if (it != problems.end() && it->second.solved) {
                if (includeFrozen || !it->second.frozen) {
                    times.push_back(it->second.solveTime);
                }
            }
        }
        sort(times.begin(), times.end(), greater<int>());
        return times;
    }
};

class ICPCSystem {
private:
    map<string, Team> teams;
    bool competitionStarted = false;
    int durationTime = 0;
    int problemCount = 0;
    bool frozen = false;
    bool flushed = false;

    // Team order after last flush (for ranking queries)
    vector<string> teamOrder;

    // Compare two teams for ranking (returns true if a ranks higher than b)
    bool compareTeams(const string& a, const string& b, bool includeFrozen = true) const {
        auto itA = teams.find(a);
        auto itB = teams.find(b);

        const Team& teamA = itA->second;
        const Team& teamB = itB->second;

        int solvedA = teamA.getSolvedCount(problemCount, includeFrozen);
        int solvedB = teamB.getSolvedCount(problemCount, includeFrozen);
        if (solvedA != solvedB) return solvedA > solvedB;

        int penaltyA = teamA.getTotalPenalty(problemCount, includeFrozen);
        int penaltyB = teamB.getTotalPenalty(problemCount, includeFrozen);
        if (penaltyA != penaltyB) return penaltyA < penaltyB;

        vector<int> timesA = teamA.getSolveTimes(problemCount, includeFrozen);
        vector<int> timesB = teamB.getSolveTimes(problemCount, includeFrozen);

        for (size_t i = 0; i < min(timesA.size(), timesB.size()); i++) {
            if (timesA[i] != timesB[i]) return timesA[i] < timesB[i];
        }

        return a < b;  // Lexicographic comparison of names
    }

    void flushScoreboard() {
        teamOrder.clear();
        for (const auto& p : teams) {
            teamOrder.push_back(p.first);
        }
        sort(teamOrder.begin(), teamOrder.end(),
             [this](const string& a, const string& b) {
                 return this->compareTeams(a, b, false);  // Exclude frozen problems for ranking
             });
        flushed = true;
    }

    void printTeamStatus(const string& name, int rank, bool isScroll = false, bool beforeScroll = false) const {
        const Team& team = teams.at(name);
        int solved = team.getSolvedCount(problemCount, !beforeScroll);
        int penalty = team.getTotalPenalty(problemCount, !beforeScroll);

        cout << name << " " << rank << " " << solved << " " << penalty;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            cout << " ";
            auto it = team.problems.find(p);
            if (it == team.problems.end() || (it->second.wrongAttempts == 0 && !it->second.solved && it->second.submitAfterFreeze == 0)) {
                cout << ".";
            } else if (it->second.frozen && !beforeScroll) {
                // Frozen state: show wrong before freeze / submissions after freeze
                int wrong = it->second.wrongBeforeFreeze;
                int after = it->second.submitAfterFreeze;
                if (wrong == 0) {
                    cout << "0/" << after;
                } else {
                    cout << "-" << wrong << "/" << after;
                }
            } else if (it->second.solved) {
                // Solved (not frozen or before scroll)
                int wrong = it->second.wrongAttempts;
                if (it->second.frozen) {
                    // Use wrongBeforeFreeze for frozen problems in before scroll view
                    wrong = it->second.wrongBeforeFreeze;
                }
                if (wrong == 0) {
                    cout << "+";
                } else {
                    cout << "+" << wrong;
                }
            } else {
                // Not solved (not frozen or before scroll)
                int wrong = it->second.wrongAttempts;
                if (it->second.frozen) {
                    // Use wrongBeforeFreeze for frozen problems in before scroll view
                    wrong = it->second.wrongBeforeFreeze;
                }
                if (wrong == 0) {
                    cout << ".";
                } else {
                    cout << "-" << wrong;
                }
            }
        }
        cout << "\n";
    }

    void printScoreboard(bool beforeScroll = false) const {
        vector<string> order;
        for (const auto& p : teams) {
            order.push_back(p.first);
        }
        sort(order.begin(), order.end(),
             [this, beforeScroll](const string& a, const string& b) {
                 return this->compareTeams(a, b, !beforeScroll);
             });

        int rank = 1;
        for (size_t i = 0; i < order.size(); i++) {
            if (i > 0 && compareTeams(order[i-1], order[i], !beforeScroll)) {
                // Not tied with previous team
                rank = i + 1;
            }
            printTeamStatus(order[i], rank, true, beforeScroll);
        }
    }

    // Get the stored rank for a team (from last flush)
    int getTeamRank(const string& name) const {
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

        for (size_t i = 0; i < teamOrder.size(); i++) {
            if (teamOrder[i] == name) {
                // Calculate actual rank based on tied teams
                int rank = 1;
                for (size_t j = 0; j < i; j++) {
                    if (compareTeams(teamOrder[j], teamOrder[i], false)) {
                        rank = j + 2;
                    }
                }
                return rank;
            }
        }
        return -1;
    }

    // Find teams with frozen problems
    vector<string> getTeamsWithFrozenProblems() const {
        vector<string> result;
        for (const auto& p : teams) {
            for (int i = 0; i < problemCount; i++) {
                char prob = 'A' + i;
                auto it = p.second.problems.find(prob);
                if (it != p.second.problems.end() && it->second.frozen) {
                    result.push_back(p.first);
                    break;
                }
            }
        }
        return result;
    }

    // Get the problem with smallest letter that is frozen for a team
    char getSmallestFrozenProblem(const string& teamName) const {
        auto it = teams.find(teamName);
        if (it == teams.end()) return '\0';

        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto pit = it->second.problems.find(p);
            if (pit != it->second.problems.end() && pit->second.frozen) {
                return p;
            }
        }
        return '\0';
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // Print scoreboard before scrolling (after flushing)
        printScoreboard(true);

        // Track ranking changes
        vector<tuple<string, string, int, int>> changes;

        // Get the order before scrolling for comparison
        auto getOrder = [this](bool includeFrozen) {
            vector<string> order;
            for (const auto& p : teams) {
                order.push_back(p.first);
            }
            sort(order.begin(), order.end(),
                 [this, includeFrozen](const string& a, const string& b) {
                     return this->compareTeams(a, b, includeFrozen);
                 });
            return order;
        };

        vector<string> prevOrder = getOrder(false);

        // Process each scroll step
        while (true) {
            // Find the lowest-ranked team with frozen problems
            vector<string> teamsWithFrozen = getTeamsWithFrozenProblems();
            if (teamsWithFrozen.empty()) break;

            // Sort by current ranking (lowest first)
            sort(teamsWithFrozen.begin(), teamsWithFrozen.end(),
                 [this](const string& a, const string& b) {
                     return !this->compareTeams(a, b, true);  // Reverse order
                 });

            // The lowest ranked team
            string& teamName = teamsWithFrozen.back();
            char problem = getSmallestFrozenProblem(teamName);

            if (problem == '\0') break;

            // Unfreeze this problem
            auto& team = teams[teamName];
            auto& ps = team.problems[problem];
            ps.frozen = false;

            // If there were correct submissions after freeze, they become visible
            // Check if the team solved this problem during freeze
            // We need to track this from submissions
            for (const auto& sub : team.submissions) {
                if (sub.problem == problem && sub.afterFreeze && sub.status == Status::Accepted) {
                    if (!ps.solved) {
                        ps.solved = true;
                        ps.solveTime = sub.time;
                    }
                }
            }

            // Flush and recalculate rankings
            prevOrder = getOrder(false);

            // Find ranking changes
            // After unfreezing, get new order
            vector<string> newOrder = getOrder(true);

            // Check if this team's ranking changed compared to before
            for (size_t i = 0; i < newOrder.size(); i++) {
                if (newOrder[i] == teamName) {
                    // Find the team's old position
                    for (size_t j = 0; j < prevOrder.size(); j++) {
                        if (prevOrder[j] == teamName) {
                            if ((int)i < (int)j) {
                                // Ranking improved
                                // Find the team that was replaced (was at position i before)
                                string replacedTeam = prevOrder[i];
                                int newSolved = team.getSolvedCount(problemCount, true);
                                int newPenalty = team.getTotalPenalty(problemCount, true);
                                changes.push_back({teamName, replacedTeam, newSolved, newPenalty});
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        }

        // Print ranking changes
        for (const auto& c : changes) {
            cout << get<0>(c) << " " << get<1>(c) << " " << get<2>(c) << " " << get<3>(c) << "\n";
        }

        // Print scoreboard after scrolling
        printScoreboard(false);

        // Update frozen state and flush
        frozen = false;
        flushScoreboard();
    }

public:
    void addTeam(const string& name) {
        if (competitionStarted) {
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

    void startCompetition(int duration, int problems) {
        if (competitionStarted) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        durationTime = duration;
        problemCount = problems;
        competitionStarted = true;
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& teamName, const string& status, int time) {
        if (!competitionStarted) return;

        Status s = parseStatus(status);
        auto& team = teams[teamName];
        char p = problem[0];

        bool afterFreeze = frozen;
        team.submissions.push_back({p, s, time, afterFreeze});

        auto& ps = team.problems[p];

        if (ps.solved) {
            // Already solved, this submission doesn't affect anything on scoreboard
            return;
        }

        if (afterFreeze) {
            // Submission during freeze period
            ps.submitAfterFreeze++;
            if (s == Status::Accepted) {
                // Will be revealed during scroll
                ps.frozen = true;
            } else {
                // Wrong attempt during freeze
                ps.frozen = true;
            }
        } else {
            // Normal submission (not frozen)
            if (s == Status::Accepted) {
                ps.solved = true;
                ps.solveTime = time;
            } else {
                ps.wrongAttempts++;
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

        int rank = getTeamRank(teamName);
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
            int duration = stoi(tokens[2]);
            int problems = stoi(tokens[4]);
            system.startCompetition(duration, problems);
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
