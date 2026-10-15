#include <unity.h>

///
/// Der Runner fuer game. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_game_wird_leer_angelegt(void);
void test_game_jedes_spiel_hat_seinen_eigenen_wuerfel(void);
void test_game_ein_neues_spiel_wuerfelt_neu(void);
void test_game_move_kommando_veroeffentlicht_ein_ereignis(void);
void test_game_move_kommando_ohne_figur_meldet_ein_ergebnis(void);

void test_move_kommando_bewegt_die_figur(void);
void test_move_kommando_auf_besetztes_feld_bewegt_nicht(void);
void test_move_kommando_ueber_den_rand_bewegt_nicht(void);
void test_move_kommando_fuer_einheit_in_reserve_bewegt_nicht(void);
void test_move_kommando_frisch_aufgestellt_zieht_nicht(void);
void test_move_kommando_nur_einmal_je_zug(void);
void test_move_kommando_am_ersten_feld_blockiert_zaehlt_nicht(void);
void test_move_kommando_teilweise_blockiert_zaehlt(void);
void test_move_regel_kosten_des_gelaendes(void);
void test_move_regel_nennt_jeden_grund(void);
void test_move_regel_strasse_kostet_eins(void);
void test_move_kommando_zu_weit_bewegt_nicht(void);
void test_deploy_nicht_auf_unbegehbares_feld(void);

void test_attack_fabrik_fuellt_das_kommando(void);
void test_attack_frisch_aufgestellt_greift_nicht_an(void);
void test_attack_nur_einmal_je_zug(void);
void test_attack_und_move_sind_unabhaengig(void);
void test_attack_ohne_gueltiges_ziel_oder_figur_zaehlt_nicht(void);
void test_attack_prueft_die_reichweite(void);
void test_attack_meldet_den_angriff(void);
void test_kampf_verfehlt(void);
void test_kampf_prallt_an_der_ruestung_ab(void);
void test_kampf_durchschlag_hilft_gegen_ruestung(void);
void test_kampf_ohne_wirkung_gegen_den_widerstand(void);
void test_kampf_jeder_wirkende_schuss_kostet_einen_lebenspunkt(void);
void test_kampf_ein_mitglied_faellt_und_die_anderen_ruecken_auf(void);
void test_kampf_die_einheit_faellt_und_der_angriff_endet(void);
void test_kampf_die_letzte_einheit_faellt_und_das_spiel_endet(void);
void test_kampf_jede_waffe_in_reichweite_feuert(void);
void test_kampf_auf_ein_leeres_feld_wird_nicht_gewuerfelt(void);
void test_kampf_trifft_auch_eigene_einheiten(void);
void test_kampf_derselbe_seed_derselbe_kampf(void);
void test_kampf_deckung_des_zielfeldes_erschwert_den_treffer(void);

void test_view_zaehlt_alle_tiles(void);
void test_view_liefert_jedes_tile_mit_seiner_position(void);
void test_view_weist_ungueltige_tile_zugriffe_ab(void);
void test_view_laesst_output_bei_ablehnung_unberuehrt(void);
void test_view_zaehlt_nur_vorhandene_einheiten(void);
void test_view_zaehlt_einheiten_in_jedem_zustand(void);
void test_view_gibt_eine_kopie_heraus(void);
void test_view_findet_die_gesetzte_figur_auf_ihrem_tile(void);

void test_tile_hinzufuegen_geht_durch_bis_in_die_welt(void);
void test_tile_entfernen_macht_void(void);
void test_tile_ohne_spiel_ist_kein_absturz(void);
void test_tile_ereignisse_kommen_beim_abonnenten_an(void);
void test_tile_ohne_aenderung_kommt_kein_ereignis_an(void);

void test_weltdefinition_baut_gelaende_und_hoehen_auf(void);
void test_weltdefinition_ohne_hoehen_ist_flach(void);
void test_weltdefinition_reihenfolge_der_schluessel_ist_egal(void);
void test_weltdefinition_in_voller_groesse(void);
void test_weltdefinition_lehnt_fehlerhafte_dateien_ab(void);
void test_weltdefinition_mit_figur_im_spiel_wird_abgelehnt(void);
void test_weltdefinition_meldet_geaenderte_und_weggefallene_felder(void);
void test_weltdefinition_aus_datei(void);
void test_weltdefinition_setzt_die_siegfelder(void);
void test_weltdefinition_kennt_alle_gelaendearten(void);
void test_weltdefinition_siegfelder_nur_auf_land(void);
void test_victory_auf_dem_feld_zaehlt_zwanzig(void);
void test_victory_ohne_can_capture_zaehlt_nichts(void);
void test_victory_nachbarn_zaehlen_je_fuenf(void);
void test_victory_mehrere_felder_und_reserve(void);
void test_victory_nach_der_letzten_runde_gewinnt_wer_mehr_hat(void);
void test_victory_gleichstand_ist_unentschieden(void);
void test_victory_nach_dem_ende_wird_alles_abgelehnt(void);
void test_victory_ohne_einheiten_verliert_man(void);
void test_victory_ohne_armee_verliert_man_nicht(void);

void test_einheit_kommt_in_die_reserve_des_spielers(void);
void test_einheit_nur_fuer_wer_mitspielt(void);
void test_einheit_mit_unhaltbaren_werten_wird_abgelehnt(void);
void test_einheit_wenn_der_pool_voll_ist(void);
void test_einheit_nach_dem_ersten_zug_wird_abgelehnt(void);

void test_deploy_fabrik_fuellt_das_kommando(void);
void test_deploy_regel_nennt_jeden_grund(void);
void test_deploy_kommando_stellt_die_einheit_auf(void);
void test_deploy_kommando_fuer_fremde_einheit_aendert_nichts(void);
void test_deploy_nur_in_der_eigenen_zone(void);
void test_deploy_kommando_ausserhalb_der_zone_aendert_nichts(void);

void test_players_beitritt_legt_einen_spieler_an(void);
void test_players_ohne_benutzer_kein_spieler(void);
void test_players_austritt_zerstoert_nur_seinen_spieler(void);
void test_players_volles_spiel_legt_keinen_an(void);
void test_players_kante_beim_beitritt(void);
void test_players_aufstellzone(void);
void test_players_neue_einheit_steht_in_liste_und_reserve(void);
void test_players_aufstellen_verlaesst_die_reserve(void);
void test_players_zuordnen_und_loesen(void);
void test_players_wiederkehr_findet_seine_einheiten(void);
void test_players_end_turn_kommando_gibt_den_zug_weiter(void);

void test_zugwechsel_erster_beitritt_meldet_ihn(void);
void test_zugwechsel_abgeben_meldet_den_naechsten(void);
void test_zugwechsel_allein_abgeben_meldet_ihn_erneut(void);
void test_zugwechsel_austritt_meldet_nur_eine_aenderung(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_game_wird_leer_angelegt);
    RUN_TEST(test_game_jedes_spiel_hat_seinen_eigenen_wuerfel);
    RUN_TEST(test_game_ein_neues_spiel_wuerfelt_neu);
    RUN_TEST(test_game_move_kommando_veroeffentlicht_ein_ereignis);
    RUN_TEST(test_game_move_kommando_ohne_figur_meldet_ein_ergebnis);

    RUN_TEST(test_move_kommando_bewegt_die_figur);
    RUN_TEST(test_move_kommando_auf_besetztes_feld_bewegt_nicht);
    RUN_TEST(test_move_kommando_ueber_den_rand_bewegt_nicht);
    RUN_TEST(test_move_kommando_fuer_einheit_in_reserve_bewegt_nicht);
    RUN_TEST(test_move_kommando_frisch_aufgestellt_zieht_nicht);
    RUN_TEST(test_move_kommando_nur_einmal_je_zug);
    RUN_TEST(test_move_kommando_am_ersten_feld_blockiert_zaehlt_nicht);
    RUN_TEST(test_move_kommando_teilweise_blockiert_zaehlt);
    RUN_TEST(test_move_regel_kosten_des_gelaendes);
    RUN_TEST(test_move_regel_nennt_jeden_grund);
    RUN_TEST(test_move_regel_strasse_kostet_eins);
    RUN_TEST(test_move_kommando_zu_weit_bewegt_nicht);
    RUN_TEST(test_deploy_nicht_auf_unbegehbares_feld);

    RUN_TEST(test_attack_fabrik_fuellt_das_kommando);
    RUN_TEST(test_attack_frisch_aufgestellt_greift_nicht_an);
    RUN_TEST(test_attack_nur_einmal_je_zug);
    RUN_TEST(test_attack_und_move_sind_unabhaengig);
    RUN_TEST(test_attack_ohne_gueltiges_ziel_oder_figur_zaehlt_nicht);
    RUN_TEST(test_attack_prueft_die_reichweite);
    RUN_TEST(test_attack_meldet_den_angriff);
    RUN_TEST(test_kampf_verfehlt);
    RUN_TEST(test_kampf_prallt_an_der_ruestung_ab);
    RUN_TEST(test_kampf_durchschlag_hilft_gegen_ruestung);
    RUN_TEST(test_kampf_ohne_wirkung_gegen_den_widerstand);
    RUN_TEST(test_kampf_jeder_wirkende_schuss_kostet_einen_lebenspunkt);
    RUN_TEST(test_kampf_ein_mitglied_faellt_und_die_anderen_ruecken_auf);
    RUN_TEST(test_kampf_die_einheit_faellt_und_der_angriff_endet);
    RUN_TEST(test_kampf_die_letzte_einheit_faellt_und_das_spiel_endet);
    RUN_TEST(test_kampf_jede_waffe_in_reichweite_feuert);
    RUN_TEST(test_kampf_auf_ein_leeres_feld_wird_nicht_gewuerfelt);
    RUN_TEST(test_kampf_trifft_auch_eigene_einheiten);
    RUN_TEST(test_kampf_derselbe_seed_derselbe_kampf);
    RUN_TEST(test_kampf_deckung_des_zielfeldes_erschwert_den_treffer);

    RUN_TEST(test_view_zaehlt_alle_tiles);
    RUN_TEST(test_view_liefert_jedes_tile_mit_seiner_position);
    RUN_TEST(test_view_weist_ungueltige_tile_zugriffe_ab);
    RUN_TEST(test_view_laesst_output_bei_ablehnung_unberuehrt);
    RUN_TEST(test_view_zaehlt_nur_vorhandene_einheiten);
    RUN_TEST(test_view_zaehlt_einheiten_in_jedem_zustand);
    RUN_TEST(test_view_gibt_eine_kopie_heraus);
    RUN_TEST(test_view_findet_die_gesetzte_figur_auf_ihrem_tile);

    RUN_TEST(test_tile_hinzufuegen_geht_durch_bis_in_die_welt);
    RUN_TEST(test_tile_entfernen_macht_void);
    RUN_TEST(test_tile_ohne_spiel_ist_kein_absturz);
    RUN_TEST(test_tile_ereignisse_kommen_beim_abonnenten_an);
    RUN_TEST(test_tile_ohne_aenderung_kommt_kein_ereignis_an);

    RUN_TEST(test_weltdefinition_baut_gelaende_und_hoehen_auf);
    RUN_TEST(test_weltdefinition_ohne_hoehen_ist_flach);
    RUN_TEST(test_weltdefinition_reihenfolge_der_schluessel_ist_egal);
    RUN_TEST(test_weltdefinition_in_voller_groesse);
    RUN_TEST(test_weltdefinition_lehnt_fehlerhafte_dateien_ab);
    RUN_TEST(test_weltdefinition_mit_figur_im_spiel_wird_abgelehnt);
    RUN_TEST(test_weltdefinition_meldet_geaenderte_und_weggefallene_felder);
    RUN_TEST(test_weltdefinition_aus_datei);
    RUN_TEST(test_weltdefinition_setzt_die_siegfelder);
    RUN_TEST(test_weltdefinition_kennt_alle_gelaendearten);
    RUN_TEST(test_weltdefinition_siegfelder_nur_auf_land);
    RUN_TEST(test_victory_auf_dem_feld_zaehlt_zwanzig);
    RUN_TEST(test_victory_ohne_can_capture_zaehlt_nichts);
    RUN_TEST(test_victory_nachbarn_zaehlen_je_fuenf);
    RUN_TEST(test_victory_mehrere_felder_und_reserve);
    RUN_TEST(test_victory_nach_der_letzten_runde_gewinnt_wer_mehr_hat);
    RUN_TEST(test_victory_gleichstand_ist_unentschieden);
    RUN_TEST(test_victory_nach_dem_ende_wird_alles_abgelehnt);
    RUN_TEST(test_victory_ohne_einheiten_verliert_man);
    RUN_TEST(test_victory_ohne_armee_verliert_man_nicht);

    RUN_TEST(test_einheit_kommt_in_die_reserve_des_spielers);
    RUN_TEST(test_einheit_nur_fuer_wer_mitspielt);
    RUN_TEST(test_einheit_mit_unhaltbaren_werten_wird_abgelehnt);
    RUN_TEST(test_einheit_wenn_der_pool_voll_ist);
    RUN_TEST(test_einheit_nach_dem_ersten_zug_wird_abgelehnt);

    RUN_TEST(test_deploy_fabrik_fuellt_das_kommando);
    RUN_TEST(test_deploy_regel_nennt_jeden_grund);
    RUN_TEST(test_deploy_kommando_stellt_die_einheit_auf);
    RUN_TEST(test_deploy_kommando_fuer_fremde_einheit_aendert_nichts);
    RUN_TEST(test_deploy_nur_in_der_eigenen_zone);
    RUN_TEST(test_deploy_kommando_ausserhalb_der_zone_aendert_nichts);

    RUN_TEST(test_players_beitritt_legt_einen_spieler_an);
    RUN_TEST(test_players_ohne_benutzer_kein_spieler);
    RUN_TEST(test_players_austritt_zerstoert_nur_seinen_spieler);
    RUN_TEST(test_players_volles_spiel_legt_keinen_an);
    RUN_TEST(test_players_kante_beim_beitritt);
    RUN_TEST(test_players_aufstellzone);
    RUN_TEST(test_players_neue_einheit_steht_in_liste_und_reserve);
    RUN_TEST(test_players_aufstellen_verlaesst_die_reserve);
    RUN_TEST(test_players_zuordnen_und_loesen);
    RUN_TEST(test_players_wiederkehr_findet_seine_einheiten);
    RUN_TEST(test_players_end_turn_kommando_gibt_den_zug_weiter);

    RUN_TEST(test_zugwechsel_erster_beitritt_meldet_ihn);
    RUN_TEST(test_zugwechsel_abgeben_meldet_den_naechsten);
    RUN_TEST(test_zugwechsel_allein_abgeben_meldet_ihn_erneut);
    RUN_TEST(test_zugwechsel_austritt_meldet_nur_eine_aenderung);

    return UNITY_END();
}
