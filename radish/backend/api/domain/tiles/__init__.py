"""
Die Tile-Bilder (TerrainImage, TerrainType.image, TerrainDecor), alle aus der
Datenbank: daraus die tile.json des Clients erzeugen (document.py) und sie samt
PNGs unter Namen mit Hash fuer das Asset-Manifest des Clients bereitstellen
(publishing.py) -- wie die Einheitenbilder (api/domain/sprites/).

Anders als ein Einheitenbild ist ein Tile-Bild nie ein Entwurf: jeder
gespeicherte Stand ist gueltig, was nicht passt, weisen schon Modell und
Serializer ab. Ein Typ ohne Bild und Dekoration hat keine tile.json; der Client
zeichnet dann den Wuerfel in seiner Farbe.
"""
