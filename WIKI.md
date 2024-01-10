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
- Un Raspberry PI 4 (5 si possible) avec au moins 4 Go de RAM ;
- Raspberry Pi HQ Camera ;
- Un ordinateur quelconque (pour le serveur).

Le logiciel que je prévois d’utiliser :
- Les bibliothèques C du Raspberry PI pour le programme d’acquisition et de transmission ;
- L’éditeur de texte VSCode pour le développement

## Scénario d'utilisation

Le scénario d'utilisation principal pour lequel a été pensé le projet consiste à avoir
une Raspberry PI avec une caméra d'un côté, et de lancer un logiciel capable de lire un
flux en protocol HLS, tel que VLC (ou OBS avec une source VLC).

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
2. Documentation sur les caméra Raspberry PI : 
https://www.raspberrypi.com/documentation/accessories/camera.html
3. Site de l’éditeur VSCode : https://code.visualstudio.com/
