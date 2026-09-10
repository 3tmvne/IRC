# Cartographie technique des salons et opérateurs

Ce document décrit la chaîne complète de traitement des commandes liées aux salons dans le serveur IRC natif multiplexé via `poll()`, avec un focus sur l’architecture, la circulation des données et les raisons de conception.

Périmètre couvert :
- [inc/Channel.hpp](../inc/Channel.hpp)
- [src/Channel.cpp](../src/Channel.cpp)
- [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp)
- [src/Server.cpp](../src/Server.cpp)
- [inc/Client.hpp](../inc/Client.hpp)
- [src/Client.cpp](../src/Client.cpp)

## 1. Vue D’ensemble Du Flux

Le serveur suit un pipeline strictement non bloquant :

```text
poll()
  -> receiveData(fd)
    -> recv()
    -> Client::appendIn()
    -> processInput(fd)
      -> Client::extractLine()
      -> Parser::parse()
      -> executeCommand(fd, cmd)
        -> handleJoinCommand / handleKickCommand / handleModeCommand / handleTopicCommand / handleInvitCommand
          -> Channel mutators
          -> Channel::broadcast()
          -> Client::appendOut()
  -> updatePollFds()
  -> sendData(fd)
    -> send()
    -> Client::consumeOut()
```

Le point clé est que les handlers de commandes ne font jamais de `send()` direct sur la route métier. Ils produisent des lignes protocolaires dans la file de sortie du client, et la boucle `poll()` se charge ensuite d’émettre ces bytes quand le descripteur est writable.

## 2. Cartographie Des Appels

### 2.1 Routage central dans `Server.cpp`

La table de dispatch est dans [src/Server.cpp](../src/Server.cpp) :

```cpp
if (name == "JOIN")
    handleJoinCommand(fd, cmd.args);
else if (name == "INVITE")
    handleInvitCommand(fd, cmd.args);
else if (name == "KICK")
    handleKickCommand(fd, cmd.args);
else if (name == "TOPIC")
    handleTopicCommand(fd, cmd.args);
else if (name == "MODE")
    handleModeCommand(fd, cmd.args);
```

Ce routage est alimenté par la boucle d’événement : `poll()` -> `receiveData()` -> `processInput()` -> `executeCommand()`.

### 2.2 Fonctions `Server` impliquées dans les salons

Les fonctions qui contrôlent la logique de salon sont :

- `handleJoinCommand`
- `handleInvitCommand`
- `handleKickCommand`
- `handleTopicCommand`
- `handleModeCommand`
- `getFdByNick`
- `buildNamesList`
- `userPrefix`
- `sendNumeric`
- `broadcastToPeers`
- `removeFromChannel`

### 2.3 Méthodes `Channel` consommées par le serveur

La classe `Channel` expose le contrat minimal nécessaire au serveur :

- appartenance : `addMember`, `removeMember`, `isMember`, `isEmpty`
- opérateurs : `addOperator`, `removeOperator`, `isOperator`
- invitations : `addInvite`, `removeInvite`, `isInvited`
- modes : `setInviteOnly`, `setTopicLocked`, `setKey`, `unsetKey`, `setLimit`, `getModeString`, `hasKey`, `isInviteOnly`, `isTopicLocked`, `isFull`
- diffusion : `broadcast`

## 3. Pourquoi `Channel` Stocke Des Fd Bruts

Dans [inc/Channel.hpp](../inc/Channel.hpp), `Channel` conserve des `std::vector<int>` pour `_members`, `_operators` et `_invited`.

### 3.1 Choix Structurel

Ce choix signifie que le salon ne possède jamais les objets `Client`. Il ne garde que des identifiants de connexion, donc des références courtes et banalisées par le noyau.

Conséquences :

- aucune propriété de durée de vie n’est transférée au salon
- aucune relation d’ownership circulaire n’existe entre salon et client
- une copie de `Channel` reste triviale et sûre
- la disparition d’un client n’oblige pas à re-tisser des pointeurs dans les salons

### 3.2 Prévention Des Crashs Lors De Fermetures Inattendues

La fermeture d’un socket depuis le terminal ou par le pair déclenche `closeClient()` dans [src/Server.cpp](../src/Server.cpp).

Le serveur retire ensuite l’entrée de `_clients`. Si `Channel` stockait des `Client*`, chaque salon pourrait garder un pointeur désormais invalide vers un objet détruit ou déplacé. Le prochain parcours de membres provoquerait un use-after-free ou un crash.

Avec des `int` :

- le salon ne détient rien de vivant
- la résolution réelle se fait à l’envoi, via `_clients.find(fd)`
- si le fd n’existe plus, l’entrée est simplement ignorée

Cela rend la structure robuste face aux déconnexions asynchrones, ce qui est exactement le cas d’usage d’un serveur multiplexé via `poll()`.

## 4. Pourquoi `Channel::broadcast` N’Envoie Pas Directement Sur Le Socket

La méthode est définie dans [src/Channel.cpp](../src/Channel.cpp) :

```cpp
void Channel::broadcast(std::map<int, Client> &clients, const std::string &message, int except) const
{
    for (size_t i = 0; i < _members.size(); i++)
    {
        if (_members[i] == except)
            continue ;
        std::map<int, Client>::iterator it = clients.find(_members[i]);
        if (it != clients.end())
            it->second.appendOut(message + "\r\n");
    }
}
```

### 4.1 Rôle Réel

`broadcast()` ne fait pas de transport réseau. Elle ne fait qu’empiler une ligne protocolée dans la file de sortie du client.

### 4.2 Pourquoi C’Est Le Bon Modèle

- `send()` peut être partiel
- `send()` peut bloquer sur un peer lent
- un handler de salon ne doit jamais dépendre de l’état de congestion d’un autre socket
- la boucle principale sait déjà quand un fd est writable grâce à `poll()`

Le modèle correct est donc : produire la ligne tout de suite, l’émettre plus tard dans `sendData()`.

### 4.3 Contrat Avec `Client`

Le tampon de sortie est porté par `Client` dans [inc/Client.hpp](../inc/Client.hpp) et [src/Client.cpp](../src/Client.cpp) :

- `appendOut()` ajoute les données
- `getOut()` expose le tampon à `sendData()`
- `consumeOut()` retire ce qui a été écrit
- `hasOut()` pilote l’activation de `POLLOUT`

Le serveur ne fait donc jamais un envoi “réactif” dans la logique métier. Il planifie une émission.

## 5. JOIN

Handler : [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp)

### 5.1 Étapes De Traitement

```cpp
if (params.empty())
    sendNumeric(fd, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
```

Ensuite :

1. découpage de la liste de salons avec `splitList(params[0], ',')`
2. découpage parallèle des clés éventuelles avec `splitList(params[1], ',')`
3. boucle sur chaque salon demandé
4. validation du masque `#` ou `&`
5. création du salon s’il n’existe pas
6. contrôle des restrictions : invite-only, key, limite
7. ajout du membre
8. retrait de l’invitation si nécessaire
9. diffusion du JOIN et envoi des numerics de bootstrap

### 5.2 Pourquoi Le Code Crée Le Salon Au Premier JOIN

Quand `_channels.find(name) == _channels.end()`, le serveur fait :

```cpp
_channels[name] = Channel(name);
_channels[name].addMember(fd);
_channels[name].addOperator(fd);
```

Le premier joiner devient opérateur pour éviter un salon sans gestionnaire initial. Sans cela, aucun utilisateur ne pourrait ajuster les modes ou contrôler le salon fraîchement créé.

### 5.3 Pourquoi Les Checks Sont Ordonnés Ainsi

L’ordre des contrôles évite du travail inutile et préserve la logique IRC :

- on rejette d’abord un masque invalide
- on refuse ensuite un salon interdit par `+i`
- on compare la clé ensuite
- on teste enfin la limite

Le salon n’est modifié qu’après toutes les validations.

### 5.4 Effets De Bord Après Succès

Après ajout au salon :

- `Channel::broadcast(... JOIN ...)` notifie tous les membres
- `RPL_NOTOPIC` ou `RPL_TOPIC` est envoyé au nouvel arrivant
- `RPL_NAMREPLY` expose la liste des membres avec le préfixe `@` pour les opérateurs
- `RPL_ENDOFNAMES` ferme la séquence

## 6. INVITE

Le handler vérifie :

1. le nombre minimal d’arguments
2. l’existence du salon
3. la présence de l’émetteur dans le salon
4. son statut d’opérateur
5. l’existence du nickname cible
6. l’absence déjà membre

Puis :

```cpp
channel.addInvite(targetFd);
sendNumeric(fd, RPL_INVITING, nick + " " + name);
_clients[targetFd].appendOut(userPrefix(fd) + " INVITE " + nick + " :" + name + "\r\n");
```

Le salon ne reçoit donc pas la mutation d’adhésion, seulement une invitation temporaire référençée par fd.

## 7. KICK

Handler : [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp)

### 7.1 Séquence Métier

1. vérifier les paramètres
2. vérifier l’existence du salon
3. vérifier que l’émetteur est membre
4. vérifier qu’il est opérateur
5. résoudre le nickname expulsé en fd
6. vérifier que la cible est bien dans le salon
7. diffuser le `KICK`
8. retirer la cible avec `removeMember`
9. supprimer le salon s’il devient vide

### 7.2 Pourquoi La Diffusion Précède Le Retrait

L’ordre est volontaire :

```cpp
channel.broadcast(_clients, userPrefix(fd) + " KICK " + name + " " + nick + " :" + reason, -1);
channel.removeMember(targetFd);
```

Raisons :

- la cible expulsée doit recevoir le message tant qu’elle est encore membre
- les autres membres doivent voir l’événement sur l’ancien état du salon
- `removeMember()` purge ensuite les listes membres/opérateurs/invités sans perturber la diffusion déjà préparée

## 8. TOPIC

`TOPIC` est simple mais important pour le modèle de verrouillage.

### 8.1 Consultation

Si aucun sujet n’est fourni, le serveur renvoie `RPL_NOTOPIC` ou `RPL_TOPIC` selon l’état du salon.

### 8.2 Modification

Si un nouveau sujet est fourni :

- l’émetteur doit être membre
- si `+t` est actif, il doit aussi être opérateur
- `channel.setTopic(params[1])` modifie l’état
- `channel.broadcast(... TOPIC ...)` notifie tout le monde

## 9. MODE

Handler : [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp)

### 9.1 Modèle De Parsing

`MODE` lit une chaîne compacte, caractère par caractère, et fait varier un état `adding` selon `+` ou `-`.

Extrait :

```cpp
std::string modes = params[1];
size_t argIndex = 2;
bool adding = true;
std::string appliedModes = "";
std::string appliedArgs = "";
char lastSign = 0;
```

Le rôle de `argIndex` est central : il pointe vers le prochain argument encore non consommé. Les modes qui n’ont pas besoin d’argument ne le touchent pas. Les modes qui en ont besoin l’avancent seulement quand ils utilisent effectivement le token.

### 9.2 Gestion Des Modes Sans Argument

- `i` appelle `setInviteOnly(adding)`
- `t` appelle `setTopicLocked(adding)`

Ces modes sont purement booléens et ne consomment rien dans `params` au-delà de la chaîne de modes elle-même.

### 9.3 Mode `k`

- en ajout : il exige un argument et appelle `setKey(params[argIndex])`
- en suppression : il appelle `unsetKey()` et ne consomme pas d’argument

Cela colle au contrat interne de `Channel`, où la clé et la présence de clé sont séparées par `_key` et `_hasKey`.

### 9.4 Mode `o`

- il exige toujours un argument
- le token est un nickname, résolu par `getFdByNick()`
- si la cible n’existe pas ou n’est pas membre, le handler émet `ERR_USERNOTINCHANNEL`
- sinon il ajoute ou retire l’opérateur

Le code consomme l’argument seulement quand il a traité un fd cible valide.

### 9.5 Mode `l`

- en ajout : il exige un argument, convertit en entier avec `std::atoi`, puis appelle `setLimit()` si la valeur est positive
- en suppression : il appelle `setLimit(0)`

La limite `0` signifie pas de limite.

### 9.6 Pourquoi Les Erreurs N’Annulent Pas Tout

Le handler tolère un traitement partiel :

- un argument manquant produit `ERR_NEEDMOREPARAMS`, mais le scan continue
- un mode inconnu produit `ERR_UNKNOWNMODE`, mais n’empêche pas les autres modes valides de s’appliquer
- une cible invalide pour `o` n’empêche pas les autres modes valides d’être enregistrés

La diffusion finale n’a lieu que si au moins un mode valide a été appliqué :

```cpp
if (!appliedModes.empty())
    channel.broadcast(_clients, userPrefix(fd) + " MODE " + name + " " + appliedModes + appliedArgs, -1);
```

### 9.7 Pourquoi Cette Construction Est Solide

Cette approche permet une syntaxe IRC compacte comme `+itk` ou `-o+l` sans rigidifier le parsing autour d’un argument fixe par mode. Chaque lettre décide localement si elle consomme ou non un argument, ce qui rend la lecture naturelle et extensible.

## 10. Contrat De Persistance Et Nettoyage

La méthode `Channel::removeMember()` retire un fd des membres, opérateurs et invités :

```cpp
removeOperator(fd);
removeInvite(fd);
```

Cette concentration est importante : la logique de nettoyage ne se duplique pas dans les handlers. Quand un client part, le serveur peut supprimer proprement ses traces de salon sans recomposer trois fois la même règle.

## 11. Ce Qu’il Faut Retenir

- `Server.cpp` est le routeur et le chef d’orchestre I/O
- `Channel` est un objet d’état pur, indexé par fd, sans ownership de `Client`
- `broadcast()` prépare le message, `sendData()` l’émet
- `JOIN`, `INVITE`, `KICK`, `TOPIC`, `MODE` modifient l’état du salon par étapes validation -> mutation -> diffusion
- `MODE` utilise un parsing incrémental avec consommation dynamique d’arguments
- `KICK` diffuse avant de retirer, pour conserver la visibilité protocolaire de l’événement

## 12. Références Directes

- [inc/Channel.hpp](../inc/Channel.hpp)
- [src/Channel.cpp](../src/Channel.cpp)
- [src/commands/ChannelCmds.cpp](../src/commands/ChannelCmds.cpp)
- [src/Server.cpp](../src/Server.cpp)
- [inc/Client.hpp](../inc/Client.hpp)
- [src/Client.cpp](../src/Client.cpp)