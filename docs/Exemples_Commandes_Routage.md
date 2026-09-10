# Exemples De Commandes Et Routage Des Vérifications

Ce document explique, commande par commande, comment le texte envoyé par le client est interprété, dans quelle fonction il arrive, quelles vérifications y sont faites, et quel scénario métier cette fonction couvre.

Le point d’entrée commun est [src/Server.cpp](../src/Server.cpp) dans `Server::executeCommand()`. Toute commande passe d’abord par :

1. lecture non bloquante dans `receiveData()`
2. extraction d’une ligne complète dans `processInput()`
3. parsing dans `Parser::parse()`
4. routage dans `executeCommand()`

Si le client n’est pas encore enregistré, le routeur ne laisse passer que `PASS`, `NICK`, `USER`, `QUIT` et `PING`.

## 1. Commandes De Base

### PASS

Exemple :

```text
PASS secret
```

Fonction cible : `Server::handlePassCommand()` dans [src/commands/Registration.cpp](../src/commands/Registration.cpp).

Vérifications :

- refuse un second PASS après enregistrement
- refuse l’absence d’argument
- refuse un mot de passe différent de celui du serveur

Scénario :

- première étape de l’authentification avant l’accès aux autres commandes
- la fonction ne complète pas la session, elle ne fait que valider le secret

### NICK

Exemple :

```text
NICK alice
```

Fonction cible : `Server::handleNickCommand()` dans [src/commands/Registration.cpp](../src/commands/Registration.cpp).

Vérifications :

- refuse un nickname vide
- refuse un format non conforme à la grammaire IRC
- refuse un nickname déjà pris par un autre client, comparaison insensible à la casse

Scénario :

- définit ou change le pseudonyme affiché dans les préfixes IRC
- peut déclencher la fin de la registration via `completeRegistration()`

### USER

Exemple :

```text
USER alice localhost irc.example :Alice Doe
```

Fonction cible : `Server::handleUserCommand()` dans [src/commands/Registration.cpp](../src/commands/Registration.cpp).

Vérifications :

- refuse un second USER après enregistrement
- refuse moins de 4 paramètres
- refuse un username vide

Scénario :

- enregistre le nom utilisateur et le vrai nom
- participe avec PASS et NICK à l’achèvement de la session via `completeRegistration()`

### PING

Exemple :

```text
PING 123456
```

Fonction cible : `Server::handlePingCommand()` dans [src/commands/Messaging.cpp](../src/commands/Messaging.cpp).

Vérifications :

- aucune validation lourde côté métier
- si aucun token n’est fourni, la réponse retombe sur le nom du serveur

Scénario :

- keepalive protocolaire pour prouver que le lien reste vivant
- le serveur répond par `PONG`

### QUIT

Exemple :

```text
QUIT :bye
```

Fonction cible : `Server::handleQuitCommand()` dans [src/commands/Messaging.cpp](../src/commands/Messaging.cpp).

Vérifications :

- aucune condition d’appartenance à un salon
- le message de sortie est facultatif

Scénario :

- annonce la fermeture volontaire de la connexion
- diffuse le départ aux pairs avant suppression des appartenances
- marque le client comme quittant pour que la boucle I/O termine proprement

## 2. Commandes De Salon

### JOIN

Exemple :

```text
JOIN #dev
```

Avec clé :

```text
JOIN #dev secret
```

Fonction cible : `Server::handleJoinCommand()` dans [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp).

Vérifications :

- refuse l’absence d’argument
- refuse un masque de salon qui ne commence pas par `#` ou `&`
- vérifie les restrictions `+i`, `+k`, `+l`
- refuse un salon déjà rejoint par le client

Scénario :

- crée le salon si nécessaire
- ajoute le premier joiner comme opérateur
- diffuse l’arrivée du membre et renvoie la liste des présents

### INVITE

Exemple :

```text
INVITE bob #dev
```

Fonction cible : `Server::handleInvitCommand()` dans [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp).

Vérifications :

- refuse moins de 2 paramètres
- refuse un salon inexistant
- refuse un émetteur absent du salon
- refuse un émetteur non opérateur
- refuse un nickname inconnu
- refuse une cible déjà membre du salon

Scénario :

- ajoute une invitation temporaire dans le salon
- notifie la cible avec un message INVITE

### KICK

Exemple :

```text
KICK #dev bob :out
```

Fonction cible : `Server::handleKickCommand()` dans [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp).

Vérifications :

- refuse moins de 2 paramètres
- refuse un salon inexistant
- refuse un émetteur absent du salon
- refuse un émetteur non opérateur
- refuse une cible inconnue ou hors du salon

Scénario :

- diffuse l’événement KICK avant retrait pour que la cible voie encore le message
- supprime ensuite le membre du salon
- supprime le salon s’il devient vide

### TOPIC

Exemple de lecture :

```text
TOPIC #dev
```

Exemple d’écriture :

```text
TOPIC #dev :nouveau sujet
```

Fonction cible : `Server::handleTopicCommand()` dans [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp).

Vérifications :

- refuse l’absence d’argument
- refuse un salon inexistant
- refuse un client non membre
- refuse l’écriture si `+t` est actif et que l’émetteur n’est pas opérateur

Scénario :

- sans second paramètre, lit le sujet courant
- avec second paramètre, modifie le sujet puis diffuse la mise à jour

### MODE

Exemple :

```text
MODE #dev +itk secret
```

Autre exemple :

```text
MODE #dev +o bob
```

Fonction cible : `Server::handleModeCommand()` dans [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp).

Vérifications :

- refuse l’absence d’argument
- refuse un salon inexistant
- si aucun mode n’est fourni, renvoie l’état actuel du salon
- refuse un client non membre pour toute modification
- refuse un client non opérateur pour toute modification
- valide dynamiquement les arguments requis par `k`, `o`, `l`
- signale les modes inconnus avec `ERR_UNKNOWNMODE`

Scénario :

- applique des modes booléens et/ou à argument dans une seule chaîne compacte
- reconstruit la diffusion seulement pour les modes réellement appliqués

### PART

Exemple :

```text
PART #dev :pause
```

Fonction cible : `Server::handlePartCommand()` dans [src/commands/Messaging.cpp](../src/commands/Messaging.cpp).

Vérifications :

- refuse l’absence de salon cible
- refuse un salon inexistant
- refuse un client non membre du salon

Scénario :

- annonce le départ au salon avant de retirer le membre
- retire ensuite le fd de toutes les listes internes via `removeFromChannel()`

## 3. Commandes De Message Direct

### PRIVMSG

Exemple :

```text
PRIVMSG bob :salut
```

Exemple vers un salon :

```text
PRIVMSG #dev :salut tout le monde
```

Fonction cible : `Server::handlePrivmsgCommand()` puis `Server::deliverMessage()` dans [src/commands/Messaging.cpp](../src/commands/Messaging.cpp).

Vérifications :

- refuse l’absence de destinataire
- refuse l’absence de texte
- pour un salon, refuse un salon inconnu
- pour un salon, refuse un émetteur non membre
- pour un nick, refuse un destinataire inconnu

Scénario :

- distribue un message à un utilisateur unique ou à tous les membres d’un salon
- utilise les files de sortie client, jamais un `send()` direct dans le handler

### NOTICE

Exemple :

```text
NOTICE bob :hello
```

Fonction cible : `Server::handleNoticeCommand()` puis `Server::deliverMessage()` dans [src/commands/Messaging.cpp](../src/commands/Messaging.cpp).

Vérifications :

- mêmes contrôles que PRIVMSG pour la résolution des cibles
- mais aucune réponse automatique d’erreur n’est générée pour les cas silencieux

Scénario :

- message one-way prévu pour les scripts et les bots
- évite les boucles de réponse automatique

## 4. Fonction Commune De Vérification

### executeCommand()

Fonction : [src/Server.cpp](../src/Server.cpp).

Rôle :

- filtre les commandes avant enregistrement
- redirige chaque nom de commande vers son handler exact
- renvoie `ERR_UNKNOWNCOMMAND` si le nom n’est pas reconnu

Scénario :

- c’est la barrière de routage centrale du serveur
- toutes les autres fonctions ne sont atteintes que si cette fonction les sélectionne

## 5. Lecture Rapide Par Commande

| Commande | Exemple | Fonction cible | Vérification dominante |
|---|---|---|---|
| PASS | `PASS secret` | `handlePassCommand()` | mot de passe et état de registration |
| NICK | `NICK alice` | `handleNickCommand()` | validité et unicité du pseudo |
| USER | `USER alice localhost irc :Alice` | `handleUserCommand()` | complétude des paramètres |
| PING | `PING 123` | `handlePingCommand()` | keepalive sans lourdeur métier |
| QUIT | `QUIT :bye` | `handleQuitCommand()` | notification de départ et nettoyage |
| JOIN | `JOIN #dev secret` | `handleJoinCommand()` | appartenance, clé, limite, invite |
| INVITE | `INVITE bob #dev` | `handleInvitCommand()` | opérateur + présence dans le salon |
| KICK | `KICK #dev bob :out` | `handleKickCommand()` | droit opérateur et membre ciblé |
| TOPIC | `TOPIC #dev :new` | `handleTopicCommand()` | lecture ou écriture du sujet |
| MODE | `MODE #dev +itk secret` | `handleModeCommand()` | parsing incrémental des modes |
| PRIVMSG | `PRIVMSG #dev :salut` | `handlePrivmsgCommand()` | destination et droits d’envoi |
| NOTICE | `NOTICE bob :hello` | `handleNoticeCommand()` | message silencieux sans boucle |
| PART | `PART #dev :pause` | `handlePartCommand()` | présence dans le salon |

## 6. Idée Directrice

Le code sépare trois niveaux :

- `Server` décide quelle fonction reçoit la commande
- la fonction de commande valide les préconditions métier
- `Channel` ou `Client` stocke l’état modifié ou la donnée à émettre

Cette séparation est ce qui rend le serveur stable en non bloquant : les vérifications sont locales, les mutations sont courtes, et les envois sont différés.