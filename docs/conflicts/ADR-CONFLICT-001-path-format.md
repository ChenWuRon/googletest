# ADR Conflict Report — 001

## Conflicting ADR

**ADR-003 Config Tree** — Path Access Model section.

## Implementation Issue

**Issue-020 ConfigDomain Tree** requires path support with format:

```
/group/controller/item
```

Example: `/my_service/cpu/cpu.max`

But ADR-003 defines the path format as:

```
/groups/<group-name>
/groups/<group-name>/controllers/<controller-type>
/groups/<group-name>/controllers/<controller-type>/items/<item-name>
```

These differ. ADR-003 inserts literal segments (`groups`, `controllers`, `items`) between names; the Issue-020 requirement builds paths directly from node names without those literal segments.

## Impact

Choosing one format affects:
- `findByPath()` path parsing logic
- `getPath()` string output
- All code that constructs or consumes tree paths (Hot Reload Diff, rule builder, serialization)
- External tooling that reads/writes paths

## Proposed Options

### Option A — Follow Issue-020 (bare node names)

Path = hierarchy of node names: `/group/controller/item`

- Simpler, shorter paths
- Matches user's explicit examples
- Path segments directly match ADR-002 naming rules
- Loss of type disambiguation in the path string

### Option B — Follow ADR-003 (literal type segments)

Path = `/groups/<name>/controllers/<type>/items/<name>`

- Self-describing paths (type information embedded in path)
- Slightly more verbose
- Matches the originally documented ADR format exactly
- Safer for serialization/deserialization without type context

### Option C — Hybrid

Internal path = bare node names (Option A). Serialization format = ADR-003 (Option B). A `ConfigPath::serialize()` method produces the long form when needed.

## Recommendation

**Option A** — implement bare node name paths as Issue-020 specifies. If the ADR-003 long form is needed later, it can be added as a serialization view on top.

## Request

Please confirm which option to proceed with.
