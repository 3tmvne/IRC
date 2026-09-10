# ft_irc — Comment le code fonctionne réellement
### Ce que tu envoies, ce que tu reçois, étape par étape

> Ce document complète la doc technique et le diaporama : il explique **le flux concret** — ce qu'un client tape au clavier (ou envoie via `nc`), ce que le serveur fait en interne ligne de code par ligne de code, et ce que chaque client connecté reçoit en retour. Tous les exemples utilisent des lignes IRC **brutes**, exactement comme elles transitent sur le socket.

---

## 0. Le modèle général : comment une commande circule

Avant de détailler chaque commande, il faut comprendre le **pipeline** commun à toutes.

```
Client (fd)                    Server                          Autres clients
    |                             |                                   |
    |--- "JOIN #chan\r\n" ------->|                                   |
    |                             |  1. poll() détecte des données    |
    |                             |     lisibles sur le fd            |
    |                             |  2. recv() -> buffer d'entrée     |
    |                             |     du Client                     |
    |                             |  3. découpe sur "\r\n" -> une     |
    |                             |     commande complète             |
    |                             |  4. parseCommand() -> nom de la   |
    |                             |     commande + params (vector)    |
    |                             |  5. dispatch vers                 |
    |                             |     handleJoinCommand(fd, params) |
    |                             |  6. le handler appelle des        |
    |                             |     méthodes de Channel/Server    |
    |                             |     et empile les réponses via    |
    |                             |     appendOut() sur CHAQUE fd     |
    |                             |     concerné                      |
    |<---------- réponse(s) ------|                                   |
    |                             |----------- broadcast() ---------->|
    |                             |  7. la boucle poll() détecte que  |
    |                             |     ces fd sont "writable" et     |
    |                             |     vide réellement leur buffer   |
    |                             |     de sortie via send()          |
```

**Points clés à retenir :**
- Rien n'est envoyé de façon synchrone au moment où tu appelles `appendOut`. Tu **empiles du texte** dans un buffer ; c'est la boucle principale (`poll`/`select`) qui, un peu plus tard, réellement `send()` ce buffer quand le socket est prêt à écrire. C'est ça, le "non-bloquant".
- Une commande peut déclencher **plusieurs sorties, vers plusieurs fd différents** : une réponse numérique pour l'émetteur, un message diffusé à tout le salon, un message privé pour une cible (cas d'`INVITE`).
- `params` est déjà nettoyé quand il arrive dans le handler : le trailing (`:texte avec espaces`) est fusionné en un seul élément du vecteur.

---

## 1. Comment tester ça toi-même (input réel)

Sans client IRC graphique, tu peux te connecter directement avec `nc` (netcat) :

```bash
nc 127.0.0.1 6667
```

Puis tu tapes des lignes texte au clavier, terminées par Entrée (qui envoie `\n` — pense à vérifier si ton serveur accepte `\n` seul ou exige `\r\n` strict). Exemple de séquence de connexion complète :

```
NICK alice
USER alice 0 * :Alice Dupont
JOIN #general
```

Tout ce que le serveur "dit" apparaîtra directement dans ton terminal `nc`. Pour simuler plusieurs clients, ouvre plusieurs terminaux avec chacun leur `nc`.

---

## 2. `JOIN` — rejoindre ou créer un salon

### Ce que le client envoie (input)

```
JOIN #general
```

ou avec une clé :

```
JOIN #general secret123
```

ou plusieurs salons d'un coup :

```
JOIN #general,#dev cle1,cle2
```

### Déroulé interne, étape par étape

1. `handleJoinCommand(fd, params)` est appelé avec `params = ["#general"]` (ou `["#general", "secret123"]`).
2. **Contrôle 1** : `params.empty()` ? Si oui → erreur `461`, fin du traitement.
3. `splitList("#general,#dev", ',')` découpe la liste de salons ; pareil pour les clés. Les deux listes sont appariées par index (le salon `i` reçoit la clé `i`, si elle existe).
4. Pour **chaque salon de la liste**, indépendamment :
   - **Contrôle 2** : le nom commence-t-il par `#` ou `&` ? Sinon → erreur `476` pour ce salon, on passe au suivant.
   - **Cas A — le salon n'existe pas encore dans `_channels`** :
     - `_channels["#general"] = Channel("#general")` (le constructeur par défaut est appelé ici, cf. doc technique).
     - `channel.addMember(fd)` puis `channel.addOperator(fd)` → **le créateur devient automatiquement opérateur**.
   - **Cas B — le salon existe déjà** :
     - `isMember(fd)` déjà vrai ? → on ignore silencieusement (pas d'erreur, pas de re-broadcast).
     - `isInviteOnly()` vrai et `isInvited(fd)` faux ? → erreur `473`.
     - `hasKey()` vrai et clé fournie différente de `getKey()` ? → erreur `475`.
     - `isFull()` vrai ? → erreur `471`.
     - Sinon : `channel.addMember(fd)`, et si le fd était invité, `channel.removeInvite(fd)` (consommation de l'invitation).
5. Une fois le membre ajouté avec succès :
   - `channel.broadcast(userPrefix(fd) + " JOIN #general\r\n", -1, clients)` → **tout le monde reçoit**, y compris le nouvel arrivant lui-même (`except = -1`).
   - Envoi **uniquement au nouvel arrivant** du topic : `331` si vide, `332` sinon.
   - Envoi **uniquement au nouvel arrivant** de la liste des membres : `353` (une ou plusieurs lignes) puis `366` pour clore.

### Ce que chaque client reçoit (output)

Si **Alice** (déjà dans `#general`, opératrice) est présente et que **Bob** fait `JOIN #general` :

**Bob reçoit** (dans l'ordre) :
```
:bob!bob@localhost JOIN #general
:server.local 332 bob #general :Bienvenue sur le salon general
:server.local 353 bob = #general :@alice bob
:server.local 366 bob #general :End of /NAMES list.
```

**Alice reçoit** (seulement) :
```
:bob!bob@localhost JOIN #general
```

### Cas d'erreur — exemple concret

Bob tente de rejoindre un salon `+k` sans la bonne clé :

*Input de Bob* :
```
JOIN #general mauvaise_cle
```

*Output reçu par Bob uniquement* :
```
:server.local 475 bob #general :Cannot join channel (+k)
```
Rien n'est diffusé aux autres membres — l'échec est silencieux pour eux.

---

## 3. `INVITE` — inviter un utilisateur

### Ce que le client envoie (input)

```
INVITE bob #general
```

### Déroulé interne

1. `handleInvitCommand(fd, params)` avec `params = ["bob", "#general"]`.
2. **Contrôle 1** : deux paramètres présents ? Sinon `461`.
3. **Contrôle 2** : `#general` existe dans `_channels` ? Sinon `403`.
4. **Contrôle 3** : `channel.isMember(fd)` (l'émetteur, Alice) ? Sinon `442`.
5. **Contrôle 4** : `channel.isOperator(fd)` ? Sinon `482`.
6. **Contrôle 5** : `getFdByNick("bob")` retourne un fd valide (≠ -1) ? Sinon `401`.
7. **Contrôle 6** : `channel.isMember(fdBob)` déjà vrai ? Si oui → `443`.
8. Si tout passe : `channel.addInvite(fdBob)`, puis :
   - Confirmation `341` envoyée à Alice (l'émetteur).
   - Message `INVITE` envoyé **uniquement** à Bob (pas de broadcast à tout le salon — c'est une notification privée).

### Ce que chaque client reçoit (output)

**Alice** (émettrice) reçoit :
```
:server.local 341 alice bob #general
```

**Bob** (invité) reçoit :
```
:alice!alice@localhost INVITE bob :#general
```

Les autres membres du salon **ne reçoivent rien**.

### Cas d'erreur — exemple concret

Alice n'est pas opératrice et tente d'inviter :

*Input* :
```
INVITE bob #general
```

*Output reçu par Alice uniquement* :
```
:server.local 482 alice #general :You're not channel operator
```

---

## 4. `KICK` — expulser un membre

### Ce que le client envoie (input)

```
KICK #general bob :spam répété
```

ou sans raison (le pseudo de l'émetteur sera utilisé par défaut) :

```
KICK #general bob
```

### Déroulé interne

1. `handleKickCommand(fd, params)` avec `params = ["#general", "bob", "spam répété"]`.
2. **Contrôle 1** : au moins 2 paramètres ? Sinon `461`.
3. **Contrôle 2** : salon existant ? Sinon `403`.
4. **Contrôle 3** : émetteur membre (`isMember(fd)`) ? Sinon `442`.
5. **Contrôle 4** : émetteur opérateur (`isOperator(fd)`) ? Sinon `482`.
6. **Contrôle 5** : `getFdByNick("bob")` puis `channel.isMember(fdBob)` ? Sinon `441`.
7. Si tout passe, **dans cet ordre précis** :
   - a. `channel.broadcast(userPrefix(fd) + " KICK #general bob :raison\r\n", -1, clients)` → diffusé à **tous, Bob inclus**, car il est encore membre à ce moment précis.
   - b. `channel.removeMember(fdBob)` → Bob quitte `_members`, `_operators`, `_invited`.
   - c. Si `channel.isEmpty()` → suppression du salon de `_channels`.

### Ce que chaque client reçoit (output)

Alice (opératrice) kick Bob du salon `#general` (Charlie est aussi présent) :

**Tout le monde dans le salon reçoit** (Alice, Bob et Charlie, avant que Bob ne soit retiré) :
```
:alice!alice@localhost KICK #general bob :spam répété
```

Après ce message, Bob n'est plus dans `_members` — s'il tape à nouveau une commande liée à `#general`, il obtiendra `442 ERR_NOTONCHANNEL`.

### Cas d'erreur — exemple concret

Charlie (simple membre, pas opérateur) tente un KICK :

*Input de Charlie* :
```
KICK #general bob
```

*Output reçu par Charlie uniquement* :
```
:server.local 482 charlie #general :You're not channel operator
```

---

## 5. `TOPIC` — lire ou changer le sujet

### 5.1 Lecture (input sans second paramètre)

*Input* :
```
TOPIC #general
```

**Déroulé** : `461` si pas de nom de salon → `403` si salon inexistant → `442` si émetteur non-membre → sinon lecture de `_topic`.

*Output si le topic est vide* (reçu uniquement par l'émetteur) :
```
:server.local 331 alice #general :No topic is set
```

*Output si le topic est défini* :
```
:server.local 332 alice #general :Bienvenue sur le salon general
```

### 5.2 Écriture (input avec second paramètre)

*Input* :
```
TOPIC #general :Nouveau sujet du jour
```

**Déroulé** :
1. `461` → `403` → `442` (mêmes contrôles de base).
2. Si `channel.isTopicLocked()` (mode `+t`) et `!channel.isOperator(fd)` → `482`.
3. Sinon : `channel.setTopic("Nouveau sujet du jour")`, puis **diffusion à tout le salon** (pas de réponse numérique à l'émetteur seul — tout le monde, émetteur inclus, reçoit la même ligne pour rester synchronisé).

*Output reçu par TOUT le salon* :
```
:alice!alice@localhost TOPIC #general :Nouveau sujet du jour
```

### Cas d'erreur

Bob (non-opérateur) tente de changer le topic d'un salon `+t` :

*Input de Bob* :
```
TOPIC #general :Je décide du sujet
```

*Output reçu par Bob uniquement* :
```
:server.local 482 bob #general :You're not channel operator
```

---

## 6. `MODE` — consulter ou modifier les réglages du salon

### 6.1 Consultation (input sans chaîne de modes)

*Input* :
```
MODE #general
```

**Déroulé** : `461` → `403`, puis renvoi direct de `getModeString()` via `324`.

*Output (émetteur uniquement)* :
```
:server.local 324 alice #general +itk clesecrete
```

### 6.2 Modification (input avec chaîne de modes)

*Input* :
```
MODE #general +okl bob clesecrete 10
```

Ici : `+o bob` (donne l'opérat à Bob), `+k clesecrete` (définit la clé), `+l 10` (limite à 10 membres) — tout en une seule commande.

**Déroulé interne, caractère par caractère** :
1. `461` → `403` (contrôles de base).
2. `442` (émetteur membre) → `482` (émetteur opérateur) — **une seule fois pour toute la commande**, pas répété par flag.
3. Parcours de la chaîne `"+okl"` :
   - `+` → `adding = true`.
   - `o` → consomme l'argument suivant (`bob`) → `getFdByNick("bob")` → si absent, `441` pour ce flag précis, sinon `channel.addOperator(fdBob)` et le flag est noté comme "appliqué".
   - `k` → consomme l'argument suivant (`clesecrete`) → `channel.setKey("clesecrete")`, `_hasKey = true`.
   - `l` → consomme l'argument suivant (`10`) → `std::atoi("10")` → `channel.setLimit(10)` (une valeur ≤ 0 serait ignorée, le flag ne serait alors pas appliqué).
4. Tous les flags/arguments **réellement appliqués** sont accumulés puis diffusés **en une seule ligne** `MODE`.

*Output diffusé à tout le salon* :
```
:alice!alice@localhost MODE #general +okl bob clesecrete 10
```

### Cas particulier — flag inconnu au milieu d'une chaîne

*Input* :
```
MODE #general +iz
```

`i` est appliqué normalement. `z` n'existe pas :

*Output reçu par l'émetteur uniquement, en plus de la diffusion du reste* :
```
:server.local 472 alice #general z :is unknown mode char to me
```

*Output diffusé à tout le salon (seul `i` a été réellement appliqué)* :
```
:alice!alice@localhost MODE #general +i
```

### Cas d'erreur — cible absente pour `+o`

*Input* :
```
MODE #general +o inconnu
```

*Output reçu par l'émetteur uniquement* :
```
:server.local 441 alice inconnu #general :They aren't on that channel
```

---

## 7. Scénario complet de bout en bout

Pour bien visualiser l'enchaînement, voici une session complète avec 3 clients (Alice, Bob, Charlie), dans l'ordre chronologique. Chaque bloc montre **qui tape quoi** et **qui reçoit quoi**.

| # | Action (input) | Qui reçoit quoi (output) |
|---|---|---|
| 1 | Alice : `JOIN #general` | Alice : `JOIN` (elle-même) + `331` (pas de topic) + `353`/`366`. Salon créé, Alice = op. |
| 2 | Bob : `JOIN #general` | Bob : `JOIN` + `331` + `353` (`@alice bob`) + `366`. Alice : `JOIN` de Bob. |
| 3 | Alice : `TOPIC #general :Discussion générale` | Alice + Bob : `TOPIC #general :Discussion générale` |
| 4 | Alice : `INVITE charlie #general` | Alice : `341`. Charlie : `INVITE` privé. Bob : rien. |
| 5 | Charlie : `JOIN #general` | Charlie : `JOIN` + `332` (topic existe déjà) + `353`/`366`. Alice + Bob : `JOIN` de Charlie. |
| 6 | Alice : `MODE #general +t` | Alice + Bob + Charlie : `MODE #general +t` |
| 7 | Bob : `TOPIC #general :Je change` | Bob uniquement : `482` (refusé, `+t` actif, Bob non-op) |
| 8 | Alice : `KICK #general bob :hors-sujet` | Alice + Bob + Charlie : `KICK #general bob :hors-sujet`, puis Bob retiré du salon |
| 9 | Charlie : `PART #general` | Alice : `PART` de Charlie. Salon devient vide → supprimé de `_channels`. |

Ce tableau est une bonne base pour une démo live en soutenance : il montre en une seule séquence **la création, les droits, les erreurs et le nettoyage mémoire**.

---

## 8. Résumé visuel du "qui reçoit quoi"

| Commande | Émetteur reçoit | Cible reçoit | Reste du salon reçoit |
|---|---|---|---|
| `JOIN` (succès) | JOIN + topic + names | — | JOIN |
| `INVITE` (succès) | `341` confirmation | `INVITE` privé | rien |
| `KICK` (succès) | KICK (broadcast) | KICK (broadcast, avant retrait) | KICK (broadcast) |
| `TOPIC` lecture | `331`/`332` | — | rien |
| `TOPIC` écriture (succès) | TOPIC (broadcast) | — | TOPIC (broadcast) |
| `MODE` lecture | `324` | — | rien |
| `MODE` écriture (succès) | MODE (broadcast) | — | MODE (broadcast) |
| Toute erreur (4xx) | code d'erreur, à lui seul | — | rien |

**Règle générale à retenir** : les erreurs sont toujours strictement privées (un seul `sendNumeric` vers l'émetteur) ; les succès qui changent l'état visible du salon (JOIN, KICK, TOPIC, MODE) sont toujours diffusés à tout le salon via `broadcast` ; seul `INVITE` fait exception en restant un échange privé entre l'émetteur et la cible.
