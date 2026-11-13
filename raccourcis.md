# Raccourcis Erable

Chemins et réglages utiles sous Windows. Collez un chemin dans la barre d'adresse de
l'Explorateur, ou dans Exécuter (Win + R).

## Dossiers

| Quoi | Chemin |
|---|---|
| Journaux (`eden_log.txt`) | `%APPDATA%\erable\log` |
| Réglages (`qt-config.ini`) | `%APPDATA%\erable\config` |
| Cache de shaders, un dossier par jeu | `%APPDATA%\erable\shader` |
| Cache de shaders de Zelda Ocarina of Time | `%APPDATA%\erable\shader\0400486024dec000` |
| Rapports de compatibilité | `%APPDATA%\erable\reports` |
| Toutes les données | `%APPDATA%\erable` |

Le journal n'est complet qu'une fois Erable fermé.

## Lanceur de diagnostic

Fichier `erable-trace.bat`, à côté de `erable.exe` :

```bat
set EDEN_NX2_STUTTER_TRACE=1
start "" erable.exe
```

Lignes à ajouter avant `start` selon le test :

| Ligne | Effet |
|---|---|
| `set EDEN_NX2_ASYNC_TRANSLATION=0` | Coupe la traduction des nouveaux shaders hors du fil graphique, pour comparer |
| `set EDEN_NX2_PARALLEL_SWIZZLE=0` | Prépare les grosses textures sur un seul cœur, comme avant (en cas de blocage au chargement) |
| `set EDEN_NX2_AUDIO_LEVEL=1` | Niveau du son envoyé par le jeu et volumes appliqués, toutes les 10 s (jeu muet) |
| `set EDEN_NX2_TC_LOG=1` | Statistiques du cache de textures toutes les 600 images : images créées et supprimées, envois de textures, ASTC |
| `set EDEN_NX2_FS_READ_TRACE=1` | Mesure les lectures de fichiers du jeu (temps de chargement) |
| `set EDEN_NX2_PARALLEL_POOL_KIB=1024` | Remet l'ancienne limite de la traduction parallèle, pour comparer |

## Menus

| Quoi | Où |
|---|---|
| Réglages recommandés, et retour en arrière | Aide → Appliquer les réglages recommandés |
| Langue de l'interface | Émulation → Configurer → Général → Interface |
| Précision GPU, shaders asynchrones | Émulation → Configurer → Graphismes → Avancé |
| Dossier du cache d'un jeu | Clic droit sur le jeu → Ouvrir le cache de pipelines transférable |
| Rapport de compatibilité | Clic droit sur le jeu → Rapport de compatibilité… |
| Fermer le jeu sans quitter l'émulateur | Bouton ✕ de la fenêtre → Fermer le jeu, ou Émulation → Arrêter (F5), ou L + Plus + Moins sur la manette |
| Afficher le jeu en cours sur Discord | Émulation → Configurer → Général → Web |

## Liens

- Lancer un build : https://github.com/erablesimu/erable/actions/workflows/ci.yml
- Versions publiées : https://github.com/erablesimu/erable/releases
- Notes de version : `PATCHNOTES.md`
- Pistes et mesures : `docs/banst-pistes.md`
