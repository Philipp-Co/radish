-- Legt die beiden Benutzer der Backend-Datenbank und das Schema der
-- Anwendung an (Service "backend-db" in docker-compose.yaml, gemountet nach
-- /docker-entrypoint-initdb.d). Das Postgres-Image fuehrt das nur auf einem
-- leeren Volume aus, einmal beim Anlegen der Datenbank: per psql als
-- POSTGRES_USER, verbunden mit POSTGRES_DB, mit ON_ERROR_STOP.
--
--   radish_owner  Besitzer der Datenbank und des Schemas RADISH_DB_SCHEMA:
--                 darf Tabellen anlegen und aendern. Nur fuer den Migrator
--                 (Service "backend-migrate").
--   radish_app    Darf Daten lesen und schreiben, aber das Schema nicht
--                 aendern. Fuer das Backend (Service "backend").
--
-- Das Schema legt nur dieses Skript an; welches die Anwendung benutzt,
-- bestimmt sie selbst ueber ihren search_path (DJANGO_DB_SCHEMA, siehe
-- radish/backend/config/database.py). Beide Namen muessen also
-- zusammenpassen. Das Schema public bleibt bestehen, die Anwendung legt dort
-- aber nichts mehr an.
--
-- POSTGRES_USER bleibt Admin der Datenbank -- fuer lokale Entwicklung und
-- Tests (config/database.py), nicht fuer die Container.
--
-- Passwoerter und Schemaname kommen aus der Umgebung des Containers
-- (\getenv, ab psql 15) und gehen per psql-Variable in die Statements, nicht
-- als Text ins Skript.

\getenv owner_pw RADISH_DB_OWNER_PASSWORD
\getenv app_pw RADISH_DB_APP_PASSWORD
\getenv schema RADISH_DB_SCHEMA

\if :{?owner_pw}
\else
    DO $$ BEGIN RAISE EXCEPTION 'RADISH_DB_OWNER_PASSWORD muss gesetzt sein.'; END $$;
\endif
\if :{?app_pw}
\else
    DO $$ BEGIN RAISE EXCEPTION 'RADISH_DB_APP_PASSWORD muss gesetzt sein.'; END $$;
\endif
\if :{?schema}
\else
    \set schema radish
\endif

SELECT current_database() AS db \gset

CREATE ROLE radish_owner LOGIN PASSWORD :'owner_pw';
CREATE ROLE radish_app LOGIN PASSWORD :'app_pw';

ALTER DATABASE :"db" OWNER TO radish_owner;

-- Ohne das duerfte sich jeder Benutzer mit der Datenbank verbinden.
REVOKE ALL ON DATABASE :"db" FROM PUBLIC;
GRANT CONNECT ON DATABASE :"db" TO radish_app;

CREATE SCHEMA :"schema" AUTHORIZATION radish_owner;
GRANT USAGE ON SCHEMA :"schema" TO radish_app;

-- Fuer alles, was radish_owner (also der Migrator) kuenftig anlegt. Djangos
-- Primaerschluessel sind Identity-Spalten; deren Sequenzen brauchen USAGE
-- fuer INSERT, SELECT fuer currval().
ALTER DEFAULT PRIVILEGES FOR ROLE radish_owner IN SCHEMA :"schema"
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO radish_app;
ALTER DEFAULT PRIVILEGES FOR ROLE radish_owner IN SCHEMA :"schema"
    GRANT USAGE, SELECT ON SEQUENCES TO radish_app;

-- Dasselbe Schema auch in template1, der Vorlage jeder neuen Datenbank:
-- `manage.py test` legt eine eigene Testdatenbank an, und die braucht das
-- Schema, sonst findet Django dort keins zum Anlegen der Tabellen. Gehoert
-- POSTGRES_USER, der die Tests faehrt.
\connect template1
CREATE SCHEMA :"schema";
