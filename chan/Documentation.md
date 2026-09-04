# Documentation des Fonctions — ft_irc / Channels

## Conventions

- `params` = liste des arguments **après** le nom de la commande, le trailing (`:texte`) étant déjà fusionné dans le dernier élément.
- Aucun `send()` : toute sortie passe par `client.appendOut("...\r\n")`.
- Les membres, opérateurs et invités sont stockés en `std::vector<int>` de file descriptors.
- C++98 strict : pas de `auto`, pas de `std::stoi` (→ `std::atoi`), pas de `std::to_string` (→ `std::stringstream`).

### Prototypes à ajouter dans `Server.hpp`

```cpp
std::string	userPrefix(int fd);
void		sendNumeric(int fd, const std::string &code, const std::string &args);
int		getFdByNick(const std::string &nick);
std::string	buildNamesList(Channel &channel);
void		removeFromChannel(int fd, const std::string &name);
void		handleJoinCommand(int fd, const std::vector<std::string> &params);
void		handleInvitCommand(int fd, const std::vector<std::string> &params);
void		handleKickCommand(int fd, const std::vector<std::string> &params);
void		handleTopicCommand(int fd, const std::vector<std::string> &params);
void		handleModeCommand(int fd, const std::vector<std::string> &params);
```

---

## 1. Classe `Channel`

| Fonction | Rôle |
|---|---|
| `Channel()` / `Channel(name)` / copie / `operator=` / `~Channel()` | Forme canonique de Coplien. Le constructeur par défaut est **obligatoire** car `std::map::operator[]` construit une valeur vide avant l'affectation. |
| `getName` / `getTopic` / `getKey` / `getMembers` / `getLimit` | Accesseurs constants ; retour par référence const pour éviter les copies. |
| `isInviteOnly` / `isTopicLocked` / `hasKey` | État des flags `+i`, `+t`, `+k`. `_hasKey` distingue « pas de clé » d'une clé vide. |
| `setTopic` / `setKey` / `unsetKey` / `setInviteOnly` / `setTopicLocked` / `setLimit` | Mutateurs appelés par TOPIC et MODE. `setLimit(0)` = pas de limite (`-l`). |
| `isMember` / `isOperator` / `isInvited` | Recherche linéaire dans les vecteurs de FD. Base de toutes les vérifications de droits. |
| `isFull` | Vrai si `_limit > 0` et que le nombre de membres l'atteint (`+l`). |
| `isEmpty` | Vrai si `_members` est vide → déclenche la suppression du salon côté serveur. |
| `addMember` / `addOperator` / `addInvite` | Ajout sans doublon (test d'appartenance avant `push_back`). |
| `removeMember` | Retire le FD des membres **et** purge son statut opérateur et son invitation : un seul appel garantit qu'aucun FD mort ne reste référencé. |
| `removeOperator` / `removeInvite` | Retraits ciblés, utilisés par `-o` et à la consommation d'une invitation. |
| `getModeString` | Construit la chaîne de modes courante (`+itkl clé limite`) pour la réponse `324 RPL_CHANNELMODEIS`. |
| `broadcast` | Parcourt les FD membres, retrouve chaque `Client` dans la map et empile le message via `appendOut` avec `\r\n`. Le paramètre `except` permet d'exclure un FD (`-1` = personne). L'I/O reste non bloquante. |

---

## 2. Helpers `Server`

| Fonction | Rôle |
|---|---|
| `userPrefix(fd)` | Construit le préfixe RFC `:nick!user@localhost` placé en tête de tout message relayé (JOIN, KICK, TOPIC, MODE, INVITE). |
| `sendNumeric(fd, code, args)` | Formate une réponse numérique `:serveur <code> <nick> <args>\r\n`. Centralise le format et évite la duplication dans les handlers. |
| `getFdByNick(nick)` | Traduit un pseudo en FD par parcours de `_clients`. Retourne `-1` si absent → base des erreurs `401` et `441`. |
| `buildNamesList(channel)` | Génère la liste des pseudos pour `353 RPL_NAMREPLY`, préfixés de `@` pour les opérateurs. |
| `removeFromChannel(fd, name)` | Retire un client d'un salon et **supprime le salon de la map s'il devient vide**. À appeler depuis `PART` et depuis la routine de déconnexion (nettoyage mémoire). |
| `splitList(str, sep)` *(static)* | Découpe une chaîne sur un séparateur via `std::stringstream` + `std::getline`. Utilisé pour `JOIN #a,#b clé1,clé2`. |

---

## 3. Les 5 handlers

### `handleJoinCommand`

1. Vérifie la présence d'au moins un paramètre (`461`).
2. Découpe la liste de salons et la liste de clés séparées par des virgules, en appariant par index.
3. Valide le préfixe du nom (`#` ou `&`) sinon `476`.
4. **Salon inexistant** : il est créé, l'auteur devient membre **et opérateur** (règle IRC : le créateur est op).
5. **Salon existant** : contrôles dans l'ordre —
   - déjà membre → ignoré silencieusement
   - `+i` sans invitation → `473 ERR_INVITEONLYCHAN`
   - `+k` avec mauvaise clé → `475 ERR_BADCHANNELKEY`
   - `+l` plein → `471 ERR_CHANNELISFULL`
   
   Puis ajout du membre et consommation de l'invitation.
6. Diffuse `JOIN` à tous les membres (le nouveau inclus, d'où `except = -1`), puis envoie au seul arrivant le sujet (`331`/`332`) et la liste des noms (`353`/`366`).

### `handleInvitCommand`

Ordre des contrôles :

| Étape | Erreur si échec |
|---|---|
| Paramètres présents | `461 ERR_NEEDMOREPARAMS` |
| Salon existant | `403 ERR_NOSUCHCHANNEL` |
| Émetteur membre | `442 ERR_NOTONCHANNEL` |
| Émetteur **opérateur** | `482 ERR_CHANOPRIVSNEEDED` |
| Cible connectée | `401 ERR_NOSUCHNICK` |
| Cible pas déjà présente | `443 ERR_USERONCHANNEL` |

Le FD invité est mémorisé dans `_invited`, ce qui l'autorisera à franchir le mode `+i`. Confirmation `341 RPL_INVITING` à l'émetteur et notification `INVITE` privée à la cible.

### `handleKickCommand`

Contrôles : `461` → `403` → émetteur membre `442` → émetteur **opérateur** `482` → cible présente dans le salon `441 ERR_USERNOTINCHANNEL`.

La raison est optionnelle et vaut par défaut le pseudo de l'émetteur. Le `KICK` est diffusé **avant** le retrait, afin que la victime reçoive elle aussi le message. Enfin, si le salon est vide, il est effacé de `_channels`.

### `handleTopicCommand`

Contrôles : `461` → `403` → émetteur membre `442`.

- **Sans second paramètre → lecture** : `331 RPL_NOTOPIC` si vide, sinon `332 RPL_TOPIC`.
- **Avec second paramètre → écriture** : si `+t` est actif et que l'émetteur n'est pas opérateur → `482`. Sinon le sujet est modifié et diffusé à tout le salon pour synchroniser les clients.

### `handleModeCommand`

Contrôles : `461` → `403`.

- **Sans chaîne de modes → consultation** : renvoie `324 RPL_CHANNELMODEIS` avec `getModeString()`.
- **Sinon** : émetteur membre (`442`) et **opérateur** (`482`) obligatoires.

La chaîne est parcourue caractère par caractère ; `+` et `-` basculent le drapeau `adding`. Un compteur `argIndex` avance uniquement sur les modes consommant un argument (`+k`, `+l`, `o` dans les deux sens), ce qui gère correctement `MODE #chan +okl bob clé 10`.

| Flag | Effet |
|---|---|
| `i` | Active/désactive le salon sur invitation seule. |
| `t` | Verrouille/déverrouille le sujet aux opérateurs. |
| `k` | `+k` définit la clé (argument requis), `-k` la supprime. |
| `o` | Donne/retire le statut opérateur ; `441` si la cible n'est pas dans le salon. |
| `l` | `+l` fixe la limite via `std::atoi` (valeur ≤ 0 ignorée), `-l` la remet à 0. |

Un mode inconnu renvoie `472 ERR_UNKNOWNMODE` sans interrompre le traitement des autres. Les modes réellement appliqués sont réassemblés dans `appliedModes` / `appliedArgs` puis diffusés en une seule ligne `MODE`, de sorte que les clients n'affichent jamais un changement qui a échoué.

---

## 4. Codes numériques utilisés

| Code | Nom | Contexte |
|---|---|---|
| `324` | `RPL_CHANNELMODEIS` | MODE en lecture |
| `331` | `RPL_NOTOPIC` | JOIN / TOPIC, sujet vide |
| `332` | `RPL_TOPIC` | JOIN / TOPIC, sujet défini |
| `341` | `RPL_INVITING` | INVITE réussi |
| `353` | `RPL_NAMREPLY` | Liste des membres après JOIN |
| `366` | `RPL_ENDOFNAMES` | Fin de la liste |
| `401` | `ERR_NOSUCHNICK` | INVITE d'un pseudo inconnu |
| `403` | `ERR_NOSUCHCHANNEL` | Salon inexistant |
| `441` | `ERR_USERNOTINCHANNEL` | KICK / MODE +o d'un non-membre |
| `442` | `ERR_NOTONCHANNEL` | L'émetteur n'est pas dans le salon |
| `443` | `ERR_USERONCHANNEL` | INVITE d'un membre déjà présent |
| `461` | `ERR_NEEDMOREPARAMS` | Arguments manquants |
| `471` | `ERR_CHANNELISFULL` | JOIN bloqué par `+l` |
| `472` | `ERR_UNKNOWNMODE` | Flag de mode inconnu |
| `473` | `ERR_INVITEONLYCHAN` | JOIN bloqué par `+i` |
| `475` | `ERR_BADCHANNELKEY` | JOIN bloqué par `+k` |
| `476` | `ERR_BADCHANMASK` | Nom de salon invalide |
| `482` | `ERR_CHANOPRIVSNEEDED` | Privilèges opérateur requis |

---

## 5. Points de soutenance

- **Pourquoi un constructeur par défaut ?** `_channels[name]` instancie un `Channel` vide avant l'affectation ; sans lui, la map ne compile pas.
- **Pourquoi `broadcast` prend la map de clients ?** Le `Channel` ne connaît que des FD, il ne stocke aucun pointeur vers un `Client` : aucun risque de pointeur pendant quand un client se déconnecte.
- **Pourquoi diffuser le KICK avant le retrait ?** La victime doit recevoir la notification, or elle ne fait plus partie de `_members` après `removeMember`.
- **Nettoyage mémoire.** Trois points d'entrée suppriment un salon vide : `handleKickCommand`, `removeFromChannel` (appelé par PART) et la routine de déconnexion.
- **Pourquoi `removeMember` purge aussi op et invite ?** Un FD est réutilisé par le système après `close()` ; un résidu ferait passer un nouveau client pour un opérateur.
