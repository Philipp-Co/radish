#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <radish/game/control/start_game.h>
#include <radish/server/interface/game_report.h>

///
/// Die Meldung des Spielendes an das Backend (interface/game_report.h): wie aus
/// Sieger und Mitspielern die Kennungen werden, und dass das Programm mit genau
/// diesen Argumenten startet. Statt register_instance.py laeuft ein kleines
/// Shell-Skript, das seine Argumente in eine Datei schreibt.
///

static const char *KENNUNG_A = "aB3xK9pQ";
static const char *KENNUNG_B = "Zz1Yy2Xx";

static void spieler(RAD_UserId_t players[2])
{
    players[0] = RAD_UserIdFromIdentifier(KENNUNG_A);
    players[1] = RAD_UserIdFromIdentifier(KENNUNG_B);
}

void test_report_sieger_und_verlierer_als_kennung(void)
{
    RAD_UserId_t players[2];
    spieler(players);

    RAD_GameReport_t report;
    TEST_ASSERT_TRUE(RAD_GameReportFromResult(players[1], players, 2, &report));
    TEST_ASSERT_EQUAL_STRING(KENNUNG_B, report.winner);
    TEST_ASSERT_EQUAL_STRING(KENNUNG_A, report.loser);

    TEST_ASSERT_TRUE(RAD_GameReportFromResult(players[0], players, 2, &report));
    TEST_ASSERT_EQUAL_STRING(KENNUNG_A, report.winner);
    TEST_ASSERT_EQUAL_STRING(KENNUNG_B, report.loser);
}

void test_report_unentschieden_nennt_niemanden(void)
{
    RAD_UserId_t players[2];
    spieler(players);

    RAD_GameReport_t report;
    TEST_ASSERT_TRUE(RAD_GameReportFromResult(RAD_USER_NONE, players, 2, &report));
    TEST_ASSERT_EQUAL_STRING("", report.winner);
    TEST_ASSERT_EQUAL_STRING("", report.loser);
}

void test_report_ohne_zwei_mitspieler_gibt_es_keinen(void)
{
    RAD_UserId_t players[2];
    spieler(players);

    RAD_GameReport_t report;
    memset(&report, 'x', sizeof(report));

    // Ein Sieger, der nicht mitspielt; nur ein Mitspieler; keiner.
    TEST_ASSERT_FALSE(RAD_GameReportFromResult(RAD_UserIdFromIdentifier("Cc3Dd4Ee"), players, 2, &report));
    TEST_ASSERT_FALSE(RAD_GameReportFromResult(players[0], players, 1, &report));
    TEST_ASSERT_FALSE(RAD_GameReportFromResult(RAD_USER_NONE, players, 0, &report));

    // "out" bleibt unangetastet.
    TEST_ASSERT_EQUAL_CHAR('x', report.winner[0]);
}

///
/// Legt ein Skript an, das seine Argumente zeilenweise nach "output" schreibt,
/// und gibt seinen Pfad zurueck.
///
static void skript(char script[64], char output[64])
{
    strcpy(script, "/tmp/radish_report_XXXXXX");
    const int fd = mkstemp(script);
    TEST_ASSERT_NOT_EQUAL(-1, fd);
    strcpy(output, script);
    strcat(output, ".out");

    char content[256];
    snprintf(content, sizeof(content), "#!/bin/sh\nfor a in \"$@\"; do echo \"$a\"; done > %s\n", output);
    TEST_ASSERT_EQUAL_INT((int)strlen(content), (int)write(fd, content, strlen(content)));
    close(fd);
    TEST_ASSERT_EQUAL_INT(0, chmod(script, 0700));
}

/// Wartet, bis die gestartete Meldung abgeholt ist -- hoechstens zwei Sekunden.
static void warte_auf_die_meldung(void)
{
    int32_t collected = 0;
    for(int32_t i = 0; (i < 200) && (collected == 0); ++i)
    {
        collected = RAD_ReportCollectFinished();
        if(collected == 0)
        {
            usleep(10000);
        }
    }
    TEST_ASSERT_EQUAL_INT32(1, collected);
}

static void lies(const char *path, char *buffer, size_t size)
{
    FILE *file = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(file);
    const size_t n = fread(buffer, 1, size - 1, file);
    buffer[n] = '\0';
    fclose(file);
}

void test_report_startet_das_programm_mit_sieger_und_verlierer(void)
{
    char script[64], output[64];
    skript(script, output);

    RAD_GameReport_t report;
    strcpy(report.winner, KENNUNG_A);
    strcpy(report.loser, KENNUNG_B);
    TEST_ASSERT_TRUE(RAD_ReportGameFinished(script, &report));
    warte_auf_die_meldung();

    char content[128];
    lies(output, content, sizeof(content));
    TEST_ASSERT_EQUAL_STRING("game-finished\naB3xK9pQ\nZz1Yy2Xx\n", content);

    unlink(script);
    unlink(output);
}

void test_report_startet_das_programm_bei_unentschieden_mit_draw(void)
{
    char script[64], output[64];
    skript(script, output);

    RAD_GameReport_t report;
    memset(&report, 0, sizeof(report));
    TEST_ASSERT_TRUE(RAD_ReportGameFinished(script, &report));
    warte_auf_die_meldung();

    char content[128];
    lies(output, content, sizeof(content));
    TEST_ASSERT_EQUAL_STRING("game-finished\ndraw\n", content);

    unlink(script);
    unlink(output);
}

///
/// Ein Programm, das es nicht gibt: kein Absturz. Je nach libc scheitert schon
/// posix_spawn, oder das Kind endet sofort mit einem Fehler -- dann holt
/// RAD_ReportCollectFinished es ab.
///
void test_report_mit_einem_programm_das_es_nicht_gibt(void)
{
    RAD_GameReport_t report;
    memset(&report, 0, sizeof(report));
    if(RAD_ReportGameFinished("/gibt/es/nicht", &report))
    {
        warte_auf_die_meldung();
    }
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReportCollectFinished());
}
