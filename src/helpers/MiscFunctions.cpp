#include "MiscFunctions.hpp"
#include "../defines.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include <set>
#include <sys/utsname.h>
#include <iomanip>
#include <sstream>
#include <execinfo.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
#if defined(__DragonFly__)
#include <sys/kinfo.h> // struct kinfo_proc
#elif defined(__FreeBSD__)
#include <sys/user.h> // struct kinfo_proc
#endif

#if defined(__NetBSD__)
#undef KERN_PROC
#define KERN_PROC  KERN_PROC2
#define KINFO_PROC struct kinfo_proc2
#else
#define KINFO_PROC struct kinfo_proc
#endif
#if defined(__DragonFly__)
#define KP_PPID(kp) kp.kp_ppid
#elif defined(__FreeBSD__)
#define KP_PPID(kp) kp.ki_ppid
#else
#define KP_PPID(kp) kp.p_ppid
#endif
#endif

static const float transforms[][9] = {
    {
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        0.0f,
        1.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        0.0f,
        -1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        0.0f,
        1.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        1.0f,
        0.0f,
        0.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
    {
        0.0f,
        -1.0f,
        0.0f,
        -1.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
    },
};

std::string absolutePath(const std::string& rawpath, const std::string& currentPath) {
    auto value = rawpath;

    if (value[0] == '.') {
        auto currentDir = currentPath.substr(0, currentPath.find_last_of('/'));

        if (value[1] == '.') {
            auto parentDir = currentDir.substr(0, currentDir.find_last_of('/'));
            value.replace(0, 2 + currentPath.empty(), parentDir);
        } else {
            value.replace(0, 1 + currentPath.empty(), currentDir);
        }
    }

    if (value[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        value.replace(0, 1, std::string(ENVHOME));
    }

    return value;
}

void addWLSignal(wl_signal* pSignal, wl_listener* pListener, void* pOwner, const std::string& ownerString) {
    ASSERT(pSignal);
    ASSERT(pListener);

    wl_signal_add(pSignal, pListener);

    Debug::log(LOG, "Registered signal for owner %lx: %lx -> %lx (owner: %s)", pOwner, pSignal, pListener, ownerString.c_str());
}

void handleNoop(struct wl_listener* listener, void* data) {
    // Do nothing
}

std::string getFormat(const char* fmt, ...) {
    char*   outputStr = nullptr;

    va_list args;
    va_start(args, fmt);
    vasprintf(&outputStr, fmt, args);
    va_end(args);

    std::string output = std::string(outputStr);
    free(outputStr);

    return output;
}

std::string escapeJSONStrings(const std::string& str) {
    std::ostringstream oss;
    for (auto& c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

void scaleBox(wlr_box* box, float scale) {
    box->width  = std::round(box->width * scale);
    box->height = std::round(box->height * scale);
    box->x      = std::round(box->x * scale);
    box->y      = std::round(box->y * scale);
}

std::string removeBeginEndSpacesTabs(std::string str) {
    if (str.empty())
        return str;

    int countBefore = 0;
    while (str[countBefore] == ' ' || str[countBefore] == '\t') {
        countBefore++;
    }

    int countAfter = 0;
    while ((int)str.length() - countAfter - 1 >= 0 && (str[str.length() - countAfter - 1] == ' ' || str[str.length() - 1 - countAfter] == '\t')) {
        countAfter++;
    }

    str = str.substr(countBefore, str.length() - countBefore - countAfter);

    return str;
}

float getPlusMinusKeywordResult(std::string source, float relative) {
    try {
        return relative + stof(source);
    } catch (...) {
        Debug::log(ERR, "Invalid arg \"%s\" in getPlusMinusKeywordResult!", source.c_str());
        return INT_MAX;
    }
}

bool isNumber(const std::string& str, bool allowfloat) {

    std::string copy = str;
    if (*copy.begin() == '-')
        copy = copy.substr(1);

    if (copy.empty())
        return false;

    bool point = !allowfloat;
    for (auto& c : copy) {
        if (c == '.') {
            if (point)
                return false;
            point = true;
            continue;
        }

        if (!std::isdigit(c))
            return false;
    }

    return true;
}

bool isDirection(const std::string& arg) {
    return arg == "l" || arg == "r" || arg == "u" || arg == "d" || arg == "t" || arg == "b";
}

int getWorkspaceIDFromString(const std::string& in, std::string& outName) {
    int result = INT_MAX;
    if (in.find("special") == 0) {
        outName = "special";

        if (in.length() > 8) {
            const auto NAME = in.substr(8);

            const auto WS = g_pCompositor->getWorkspaceByName("special:" + NAME);

            outName = "special:" + NAME;

            return WS ? WS->m_iID : g_pCompositor->getNewSpecialID();
        }

        return SPECIAL_WORKSPACE_START;
    } else if (in.find("name:") == 0) {
        const auto WORKSPACENAME = in.substr(in.find_first_of(':') + 1);
        const auto WORKSPACE     = g_pCompositor->getWorkspaceByName(WORKSPACENAME);
        if (!WORKSPACE) {
            result = g_pCompositor->getNextAvailableNamedWorkspace();
        } else {
            result = WORKSPACE->m_iID;
        }
        outName = WORKSPACENAME;
    } else if (in.find("empty") == 0) {
        int id = 0;
        while (++id < INT_MAX) {
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(id);
            if (!PWORKSPACE || (g_pCompositor->getWindowsOnWorkspace(id) == 0))
                return id;
        }
    } else if (in.find("prev") == 0) {
        if (!g_pCompositor->m_pLastMonitor)
            return INT_MAX;

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(g_pCompositor->m_pLastMonitor->activeWorkspace);

        if (!PWORKSPACE)
            return INT_MAX;

        const auto PLASTWORKSPACE = g_pCompositor->getWorkspaceByID(PWORKSPACE->m_sPrevWorkspace.iID);

        if (!PLASTWORKSPACE)
            return INT_MAX;

        outName = PLASTWORKSPACE->m_szName;
        return PLASTWORKSPACE->m_iID;
    } else {
        if (in[0] == 'r' && (in[1] == '-' || in[1] == '+') && isNumber(in.substr(2))) {
            if (!g_pCompositor->m_pLastMonitor) {
                Debug::log(ERR, "Relative monitor workspace on monitor null!");
                result = INT_MAX;
                return result;
            }
            result = (int)getPlusMinusKeywordResult(in.substr(1), 0);

            int           remains = (int)result;

            std::set<int> invalidWSes;

            // Collect all the workspaces we can't jump to.
            for (auto& ws : g_pCompositor->m_vWorkspaces) {
                if (ws->m_bIsSpecialWorkspace || (ws->m_iMonitorID != g_pCompositor->m_pLastMonitor->ID)) {
                    // Can't jump to this workspace
                    invalidWSes.insert(ws->m_iID);
                }
            }
            for (auto& rule : g_pConfigManager->getAllWorkspaceRules()) {
                const auto PMONITOR = g_pCompositor->getMonitorFromName(rule.monitor);
                if (!PMONITOR || PMONITOR->ID == g_pCompositor->m_pLastMonitor->ID) {
                    // Can't be invalid
                    continue;
                }
                // WS is bound to another monitor, can't jump to this
                invalidWSes.insert(rule.workspaceId);
            }

            // Prepare all named workspaces in case when we need them
            std::vector<int> namedWSes;
            for (auto& ws : g_pCompositor->m_vWorkspaces) {
                if (ws->m_bIsSpecialWorkspace || (ws->m_iMonitorID != g_pCompositor->m_pLastMonitor->ID) || ws->m_iID >= 0)
                    continue;

                namedWSes.push_back(ws->m_iID);
            }
            std::sort(namedWSes.begin(), namedWSes.end());

            // Just take a blind guess at where we'll probably end up
            int  predictedWSID = g_pCompositor->m_pLastMonitor->activeWorkspace + remains;
            int  remainingWSes = 0;
            char walkDir       = in[1];

            // sanitize. 0 means invalid oob in -
            predictedWSID = std::max(predictedWSID, 0);

            // Count how many invalidWSes are in between (how bad the prediction was)
            int  beginID = in[1] == '+' ? g_pCompositor->m_pLastMonitor->activeWorkspace + 1 : predictedWSID;
            int  endID   = in[1] == '+' ? predictedWSID : g_pCompositor->m_pLastMonitor->activeWorkspace;
            auto begin   = invalidWSes.upper_bound(beginID - 1); // upper_bound is >, we want >=
            for (auto it = begin; *it <= endID && it != invalidWSes.end(); it++) {
                remainingWSes++;
            }

            // Handle named workspaces. They are treated like always before other workspaces
            if (g_pCompositor->m_pLastMonitor->activeWorkspace < 0) {
                // Behaviour similar to 'm'
                // Find current
                int currentItem = -1;
                for (size_t i = 0; i < namedWSes.size(); i++) {
                    if (namedWSes[i] == g_pCompositor->m_pLastMonitor->activeWorkspace) {
                        currentItem = i;
                        break;
                    }
                }

                currentItem += remains;
                currentItem = std::max(currentItem, 0);
                if (currentItem >= (int)namedWSes.size()) {
                    // At the seam between namedWSes and normal WSes. Behave like r+[diff] at imaginary ws 0
                    int diff      = currentItem - (namedWSes.size() - 1);
                    predictedWSID = diff;
                    int  beginID  = 1;
                    int  endID    = predictedWSID;
                    auto begin    = invalidWSes.upper_bound(beginID - 1); // upper_bound is >, we want >=
                    for (auto it = begin; *it <= endID && it != invalidWSes.end(); it++) {
                        remainingWSes++;
                    }
                    walkDir = '+';
                } else {
                    // We found our final ws.
                    remainingWSes = 0;
                    predictedWSID = namedWSes[currentItem];
                }
            }

            // Go in the search direction for remainingWSes
            // The performance impact is directly proportional to the number of open and bound workspaces
            int finalWSID = predictedWSID;
            if (walkDir == '-') {
                int beginID = finalWSID;
                int curID   = finalWSID;
                while (--curID > 0 && remainingWSes > 0) {
                    if (invalidWSes.find(curID) == invalidWSes.end()) {
                        remainingWSes--;
                    }
                    finalWSID = curID;
                }
                if (finalWSID <= 0 || invalidWSes.find(finalWSID) != invalidWSes.end()) {
                    if (namedWSes.size()) {
                        // Go to the named workspaces
                        // Need remainingWSes more
                        int namedWSIdx = namedWSes.size() - remainingWSes;
                        // Sanitze
                        namedWSIdx = std::clamp(namedWSIdx, 0, (int)namedWSes.size() - 1);
                        finalWSID  = namedWSes[namedWSIdx];
                    } else {
                        // Couldn't find valid workspace in negative direction, search last first one back up positive direction
                        walkDir = '+';
                        // We know, that everything less than beginID is invalid, so don't bother with that
                        finalWSID     = beginID;
                        remainingWSes = 1;
                    }
                }
            }
            if (walkDir == '+') {
                int curID = finalWSID;
                while (++curID < INT32_MAX && remainingWSes > 0) {
                    if (invalidWSes.find(curID) == invalidWSes.end()) {
                        remainingWSes--;
                    }
                    finalWSID = curID;
                }
            }

            result                = finalWSID;
            const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(result);
            if (PWORKSPACE)
                outName = g_pCompositor->getWorkspaceByID(result)->m_szName;
            else
                outName = std::to_string(finalWSID);

        } else if ((in[0] == 'm' || in[0] == 'e') && (in[1] == '-' || in[1] == '+') && isNumber(in.substr(2))) {
            bool onAllMonitors = in[0] == 'e';

            if (!g_pCompositor->m_pLastMonitor) {
                Debug::log(ERR, "Relative monitor workspace on monitor null!");
                result = INT_MAX;
                return result;
            }

            // monitor relative
            result = (int)getPlusMinusKeywordResult(in.substr(1), 0);

            // result now has +/- what we should move on mon
            int              remains = (int)result;

            std::vector<int> validWSes;
            for (auto& ws : g_pCompositor->m_vWorkspaces) {
                if (ws->m_bIsSpecialWorkspace || (ws->m_iMonitorID != g_pCompositor->m_pLastMonitor->ID && !onAllMonitors))
                    continue;

                validWSes.push_back(ws->m_iID);
            }

            std::sort(validWSes.begin(), validWSes.end());

            // get the offset
            remains = remains < 0 ? -((-remains) % validWSes.size()) : remains % validWSes.size();

            // get the current item
            int currentItem = -1;
            for (size_t i = 0; i < validWSes.size(); i++) {
                if (validWSes[i] == g_pCompositor->m_pLastMonitor->activeWorkspace) {
                    currentItem = i;
                    break;
                }
            }

            // apply
            currentItem += remains;

            // sanitize
            if (currentItem >= (int)validWSes.size()) {
                currentItem = currentItem % validWSes.size();
            } else if (currentItem < 0) {
                currentItem = validWSes.size() + currentItem;
            }

            result  = validWSes[currentItem];
            outName = g_pCompositor->getWorkspaceByID(validWSes[currentItem])->m_szName;
        } else {
            if (in[0] == '+' || in[0] == '-') {
                if (g_pCompositor->m_pLastMonitor)
                    result = std::max((int)getPlusMinusKeywordResult(in, g_pCompositor->m_pLastMonitor->activeWorkspace), 1);
                else {
                    Debug::log(ERR, "Relative workspace on no mon!");
                    result = INT_MAX;
                }
            } else if (isNumber(in))
                result = std::max(std::stoi(in), 1);
            else {
                // maybe name
                const auto PWORKSPACE = g_pCompositor->getWorkspaceByName(in);
                if (PWORKSPACE)
                    result = PWORKSPACE->m_iID;
            }

            outName = std::to_string(result);
        }
    }

    return result;
}

float vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2) {
    const float DX = std::max({0.0, p1.x - vec.x, vec.x - p2.x});
    const float DY = std::max({0.0, p1.y - vec.y, vec.y - p2.y});
    return DX * DX + DY * DY;
}

// Execute a shell command and get the output
std::string execAndGet(const char* cmd) {
    std::array<char, 128>                          buffer;
    std::string                                    result;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        Debug::log(ERR, "execAndGet: failed in pipe");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void logSystemInfo() {
    struct utsname unameInfo;

    uname(&unameInfo);

    Debug::log(LOG, "System name: %s", unameInfo.sysname);
    Debug::log(LOG, "Node name: %s", unameInfo.nodename);
    Debug::log(LOG, "Release: %s", unameInfo.release);
    Debug::log(LOG, "Version: %s", unameInfo.version);

    Debug::log(NONE, "\n");

#if defined(__DragonFly__) || defined(__FreeBSD__)
    const std::string GPUINFO = execAndGet("pciconf -lv | fgrep -A4 vga");
#else
    const std::string GPUINFO = execAndGet("lspci -vnn | grep VGA");
#endif
    Debug::log(LOG, "GPU information:\n%s\n", GPUINFO.c_str());

    if (GPUINFO.contains("NVIDIA")) {
        Debug::log(WARN, "Warning: you're using an NVIDIA GPU. Make sure you follow the instructions on the wiki if anything is amiss.\n");
    }

    // log etc
    Debug::log(LOG, "os-release:");

    Debug::log(NONE, "%s", execAndGet("cat /etc/os-release").c_str());
}

void matrixProjection(float mat[9], int w, int h, wl_output_transform tr) {
    memset(mat, 0, sizeof(*mat) * 9);

    const float* t = transforms[tr];
    float        x = 2.0f / w;
    float        y = 2.0f / h;

    // Rotation + reflection
    mat[0] = x * t[0];
    mat[1] = x * t[1];
    mat[3] = y * t[3];
    mat[4] = y * t[4];

    // Translation
    mat[2] = -copysign(1.0f, mat[0] + mat[1]);
    mat[5] = -copysign(1.0f, mat[3] + mat[4]);

    // Identity
    mat[8] = 1.0f;
}

int64_t getPPIDof(int64_t pid) {
#if defined(KERN_PROC_PID)
    int mib[] = {
        CTL_KERN,
        KERN_PROC,
        KERN_PROC_PID,
        (int)pid,
#if defined(__NetBSD__) || defined(__OpenBSD__)
        sizeof(KINFO_PROC),
        1,
#endif
    };
    u_int      miblen = sizeof(mib) / sizeof(mib[0]);
    KINFO_PROC kp;
    size_t     sz = sizeof(KINFO_PROC);
    if (sysctl(mib, miblen, &kp, &sz, NULL, 0) != -1)
        return KP_PPID(kp);

    return 0;
#else
    std::string       dir     = "/proc/" + std::to_string(pid) + "/status";
    FILE*             infile;

    infile = fopen(dir.c_str(), "r");
    if (!infile)
        return 0;

    char*       line = nullptr;
    size_t      len  = 0;
    ssize_t     len2 = 0;

    std::string pidstr;

    while ((len2 = getline(&line, &len, infile)) != -1) {
        if (strstr(line, "PPid:")) {
            pidstr            = std::string(line, len2);
            const auto tabpos = pidstr.find_last_of('\t');
            if (tabpos != std::string::npos)
                pidstr = pidstr.substr(tabpos);
            break;
        }
    }

    fclose(infile);
    if (line)
        free(line);

    try {
        return std::stoll(pidstr);
    } catch (std::exception& e) { return 0; }
#endif
}

int64_t configStringToInt(const std::string& VALUE) {
    if (VALUE.find("0x") == 0) {
        // Values with 0x are hex
        const auto VALUEWITHOUTHEX = VALUE.substr(2);
        return stol(VALUEWITHOUTHEX, nullptr, 16);
    } else if (VALUE.find("rgba(") == 0 && VALUE.find(')') == VALUE.length() - 1) {
        const auto VALUEWITHOUTFUNC = VALUE.substr(5, VALUE.length() - 6);

        if (removeBeginEndSpacesTabs(VALUEWITHOUTFUNC).length() != 8) {
            Debug::log(WARN, "invalid length %i for rgba", VALUEWITHOUTFUNC.length());
            throw std::invalid_argument("rgba() expects length of 8 characters (4 bytes)");
        }

        const auto RGBA = std::stol(VALUEWITHOUTFUNC, nullptr, 16);

        // now we need to RGBA -> ARGB. The config holds ARGB only.
        return (RGBA >> 8) + 0x1000000 * (RGBA & 0xFF);
    } else if (VALUE.find("rgb(") == 0 && VALUE.find(')') == VALUE.length() - 1) {
        const auto VALUEWITHOUTFUNC = VALUE.substr(4, VALUE.length() - 5);

        if (removeBeginEndSpacesTabs(VALUEWITHOUTFUNC).length() != 6) {
            Debug::log(WARN, "invalid length %i for rgb", VALUEWITHOUTFUNC.length());
            throw std::invalid_argument("rgb() expects length of 6 characters (3 bytes)");
        }

        const auto RGB = std::stol(VALUEWITHOUTFUNC, nullptr, 16);

        return RGB + 0xFF000000; // 0xFF for opaque
    } else if (VALUE.find("true") == 0 || VALUE.find("on") == 0 || VALUE.find("yes") == 0) {
        return 1;
    } else if (VALUE.find("false") == 0 || VALUE.find("off") == 0 || VALUE.find("no") == 0) {
        return 0;
    }
    return std::stoll(VALUE);
}

double normalizeAngleRad(double ang) {
    if (ang > M_PI * 2) {
        while (ang > M_PI * 2)
            ang -= M_PI * 2;
        return ang;
    }

    if (ang < 0.0) {
        while (ang < 0.0)
            ang += M_PI * 2;
        return ang;
    }

    return ang;
}

std::string replaceInString(std::string subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

std::vector<SCallstackFrameInfo> getBacktrace() {
    std::vector<SCallstackFrameInfo> callstack;

    void*                            bt[1024];
    size_t                           btSize;
    char**                           btSymbols;

    btSize    = backtrace(bt, 1024);
    btSymbols = backtrace_symbols(bt, btSize);

    for (size_t i = 0; i < btSize; ++i) {
        callstack.emplace_back(SCallstackFrameInfo{bt[i], std::string{btSymbols[i]}});
    }

    return callstack;
}
