# Projet Object Connectés

Ce projet, actuellement intitulé Bike Streamer, consiste en la création d’un dispositif 
permettant de capter des images en flux continue tout en étant embarqué sur un vélo et 
permettant de retransmettre ces images vers un serveur fixe via un signal téléphonique 4G.

## Équipe

- Théo Rozier

## Problématique

Le projet vient principalement d’un challenge personnel, car les différents composants de 
ce projet sont tous intéressants à étudier : capteurs vidéo, encodage vidéo, transmission
de signal ou encore télémétrie. Il vient également du fait que je fais personnellement du
VTT, ce serait donc assez simple de le tester en conditions réelles pour déterminer au 
final si le projet est un succès ou non, car j’ai conscience que les transmissions peuvent
être très compliquées par endroit.

## Matériel utilisé

Le matériel que je prévois d’utiliser pour l’instant :
- Un Raspberry Pi 4 (5 si possible) avec au moins 4 Go de RAM ;
- Raspberry Pi HQ Camera ;
- Un ordinateur quelconque (pour le serveur).

Le logiciel que je prévois d’utiliser :
- Les bibliothèques C du Raspberry Pi pour le programme d’acquisition et de transmission ;
- L’éditeur de texte VSCode pour le développement

## Scénario d'utilisation

Le scénario d'utilisation principal pour lequel a été pensé le projet consiste à avoir
une Raspberry Pi avec une caméra d'un côté, et de lancer un logiciel capable de lire un
flux en protocol HLS, tel que VLC (ou OBS avec une source VLC).

Du côté du serveur, il faut également avoir installé [MediaMTX]. Il cette fois-ci lancer
MediaMTX en lui donnant comme seul argument le fichier `server.yml`. Cette instance permet
de recevoir le flux RTSP depuis le RPi et le proxy via un serveur HLS, plus pratique pour
lire directement depuis le web.

Sur le Raspberry Pi, il est nécessaire d'avoir au préalable installé le logiciel 
[MediaMTX], il faut ensuite simplement lancer MediaMTX en lui donnant comme seul 
argument le fichier `rpi.yml`. Un example de commande pour lancer le programme: 
`mediamtx rpi.yml`. Il peut être nécessaire de modifier les dernière lignes du fichier de
configuration pour ajuster l'adresse du serveur RTSP.

Une fois ceci fait, le RPi devrait envoyer son flux vidéo vers le serveur, et depuis le
serveur il est possible d'observer le flux directement depuis son navigateur web and
allant sur: `http://<server>:8888/cam_push`. Pour tester la source OBS, il suffit 
d'ajouter une source VLC avec pour URL: `http://<server>:8888/cam_push/index.m3u8`.

[MediaMTX]: https://github.com/bluenviron/mediamtx

## Budget

Le projet n'a pas été couteux pour moi, puisque j'avais déjà tout le matériel et j'avais
en partie déjà réfléchis à ce projet.

## Bilan

Ce projet était ambitieux, mais clairement réalisable. Malheuresement je n'ai pas su
trouver la motivation pour le réaliser jusqu'au bout. J'aurais également aimé décendre
plus bah dans le fonctionnement de linux mais celà s'est avéré être très mal documenté
avec peu de personne s'y intéressant réellement. J'ai obtenu des résultats prometteurs
mais je ne voyais pas le bout, j'ai opté pour une solution plus simple avec l'utilisation
du logiciel MediaMTX, qui gère directement ce que je voulais faire.

## Poursuite du travail

Je souhaiterais poursuivre le projet, mais sans contrainte de temps et ainsi pouvoir
reprendre mon travail plus bas niveau et ainsi y apporter de la télémétrie, ce que je
n'ai pas eu le temps de faire.

## Annexes

1. Un dépôt Git regroupant le client (acquisition et transmission) et le serveur 
(réception des images) est hebergé sur GitHub : 
https://github.com/mindstorm38/bike-streamer
2. Documentation sur les caméra Raspberry Pi : 
https://www.raspberrypi.com/documentation/accessories/camera.html
3. Site de l’éditeur VSCode : https://code.visualstudio.com/
