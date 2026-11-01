#!/bin/sh
#
# Container-Einstiegspunkt: exec supervisord -- das haelt Django und die
# Game-Instanzen nebeneinander am Leben und startet neu, was abstuerzt (siehe
# supervisord.conf, game-instance-entrypoint.sh). Kein `migrate` wie in
# docker/backend/entrypoint.sh: game-server hat keine Datenbank (siehe
# DATABASES in radish/game-server/config/settings.py).
#
set -e

exec supervisord -c /etc/supervisord.conf
