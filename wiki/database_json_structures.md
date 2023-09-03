# Database json sturctures

## tw_bots_quest -> TasksData

### _**Reward item's:**_
> Type: **json array**.
> Fields:
> * **id** - itemID from database **tw_items_list**.
> * **count** - required amount.

**Examples:**
```json
"reward_items": [
  {
    "id": 1,
    "count": 1
  }
]
```

### _**Required item's:**_
> Type: **json array**.
> Fields:
> * **id** - itemID from database **tw_items_list**.
> * **count** - required amount.
> * **?type** - type of item required. Might make a difference. **Show**, **Pickup**, **Default**. **_Default value: Default_**.

**Examples:**
```json
"required_items": [
  {
    "id": 1,
    "count": 1,
    "type": "pickup"
  }
]
```

### _**Requied defeat:**_
> Type: **json array**.
> Fields:
> * **id** - botID from database **tw_bots_info**.
> * **count** - required amount.

**Examples:**
```json
"defeat_bots": [
  {
    "id": 1,
    "count": 10
  }
]
```

### _**Required move's:**_
> Type: **json array**.
> Fields:
> * **x** - position by **X**.
> * **y** - position by **Y**.
> * **?world_id** - worldID from database **enum_worlds**. **_Default value: world in which the bot is located is set_**.
> * **?step** - is set ordered through required steps, in the unordered case the steps will be ignored. **_Default value: 1_**.
> * **?collect_item_id** - itemID from database **tw_items_list**. Get an item at the end of a move point.
> * **?navigator** - add a navigator to move to a point. **_Default value: true_**.
> * **?use_in_chat** - end the point after using the chat with the set text.
> * **?text** - send a chat msg to the player upon completion of a point.


**Examples:**
```json
"move_to": [
  {
    "step": 1,
    "x": 5690,
    "y": 6353,
    "text": "You surveyed the area and saw a suspicious spot a little further away."
  },
  {
    "x": 5962,
    "y": 7505,
    "navigator": false
  },
  {
    "step": 2,
    "x": 9791,
    "y": 8561,
    "use_in_chat": "popa",
    "text": "You pochesali popky."
  },
  {
    "x": 5891,
    "y": 7505,
    "collect_item_id": 11,
    "use_in_chat": "pick up"
  },
  {
    "step": 3,
    "world_id": 2,
    "x": 5600,
    "y": 7605,
    "collect_item_id": 13
  }
]
```
### _Everything can be merged into a single JSON object for multi task._
