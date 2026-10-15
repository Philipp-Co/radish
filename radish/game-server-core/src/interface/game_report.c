#include <radish/server/interface/game_report.h>

#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

bool RAD_GameReportFromResult(RAD_UserId_t winner,
                              const RAD_UserId_t *players,
                              int32_t number_of_players,
                              RAD_GameReport_t *out)
{
    RAD_GameReport_t report;
    memset(&report, 0, sizeof(report));

    // Ein Unentschieden nennt niemanden -- es muss aber trotzdem ein Spiel mit
    // zwei Mitspielern gewesen sein, sonst gaebe es nichts zu melden.
    if(number_of_players < 2)
    {
        return false;
    }
    if(winner == RAD_USER_NONE)
    {
        *out = report;
        return true;
    }

    bool winner_plays = false;
    RAD_UserId_t loser = RAD_USER_NONE;
    for(int32_t i = 0; i < number_of_players; ++i)
    {
        if(players[i] == winner)
        {
            winner_plays = true;
        }
        else if(loser == RAD_USER_NONE)
        {
            loser = players[i];
        }
    }
    if(!winner_plays || (loser == RAD_USER_NONE))
    {
        return false;
    }

    if(!RAD_IdentifierFromUserId(winner, report.winner) || !RAD_IdentifierFromUserId(loser, report.loser))
    {
        return false;
    }

    *out = report;
    return true;
}

bool RAD_ReportGameFinished(const char *command, const RAD_GameReport_t *report)
{
    const bool draw = (report->winner[0] == '\0');

    // posix_spawn will char * const[] -- die Zeichenketten aendert es nicht.
    char *argv[5] = {
        (char *)command,
        "game-finished",
        draw ? "draw" : (char *)report->winner,
        draw ? NULL : (char *)report->loser,
        NULL
    };

    pid_t pid = 0;
    const int result = posix_spawn(&pid, command, NULL, NULL, argv, environ);
    if(result != 0)
    {
        printf("Spielende nicht gemeldet: %s laesst sich nicht starten -- %s\n", command, strerror(result));
        return false;
    }

    if(draw)
    {
        printf("Spielende wird gemeldet (PID %d): unentschieden\n", (int)pid);
    }
    else
    {
        printf("Spielende wird gemeldet (PID %d): %s gewinnt gegen %s\n", (int)pid, report->winner, report->loser);
    }
    return true;
}

int32_t RAD_ReportCollectFinished(void)
{
    // Jedes Kind des Servers ist eine Meldung -- andere startet er nicht.
    // zucchini_server ist sein Geschwister (game-instance-entrypoint.sh), nicht
    // sein Kind.
    int32_t collected = 0;
    for(;;)
    {
        int wait_status = 0;
        const pid_t pid = waitpid(-1, &wait_status, WNOHANG);
        if(pid <= 0)
        {
            // 0: es laeuft noch eine; -1 mit ECHILD: es gibt keine.
            break;
        }

        collected++;
        if(WIFEXITED(wait_status) && (WEXITSTATUS(wait_status) == 0))
        {
            printf("Spielende gemeldet (PID %d).\n", (int)pid);
        }
        else if(WIFEXITED(wait_status))
        {
            printf("Spielende nicht gemeldet (PID %d): Exit-Code %d -- siehe Ausgabe davor.\n",
                   (int)pid, WEXITSTATUS(wait_status));
        }
        else if(WIFSIGNALED(wait_status))
        {
            printf("Spielende nicht gemeldet (PID %d): beendet durch Signal %d.\n",
                   (int)pid, WTERMSIG(wait_status));
        }
    }
    return collected;
}
