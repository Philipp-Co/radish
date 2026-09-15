#!/bin/sh
#
# Container-Einstiegspunkt: Migrationen einmalig, danach exec supervisord --
# das haelt Django und die Game-Instanz nebeneinander am Leben und startet
# neu, was abstuerzt (siehe supervisord.conf, game-instance-entrypoint.sh).
# Sqlite statt Postgres -- kein Warten auf eine Datenbank noetig, anders als
# in docker/backend/entrypoint.sh.
#
set -e

python manage.py migrate --noinput

exec supervisord -c /etc/supervisord.conf
