<?php
// Shared, typed record references. Never replace arbitrary matching numbers.
declare(strict_types=1);

function editor_reference_schema(): array
{
    static $schema;
    return $schema ??= json_decode(file_get_contents(__DIR__ . '/../editor-core/reference-schema.json'), true, 512, JSON_THROW_ON_ERROR);
}

function editor_transform_references(string $resource, array $data, callable $visit): array
{
    foreach (editor_reference_schema()[$resource] ?? [] as $rule) {
        $col = $rule['column'];
        if (!array_key_exists($col, $data)) continue;
        $ref = static function ($value, ?string $target = null) use ($visit, $rule, $col) {
            if ((!is_int($value) && !is_string($value) && !is_float($value)) || !ctype_digit((string)$value) || (float)$value > 9007199254740991) return $value;
            $mapped = $visit((int)$value, $target ?? $rule['target'], $col);
            return $mapped === (int)$value ? $value : (is_string($value) ? (string)$mapped : $mapped);
        };
        $path = static function ($node, array $parts, string $target) use (&$path, $ref) {
            if (!$parts) return $ref($node, $target);
            if (!is_array($node)) return $node;
            $key = array_shift($parts);
            foreach ($key === '*' ? array_keys($node) : [$key] as $k) {
                if (array_key_exists($k, $node)) $node[$k] = $path($node[$k], $parts, $target);
            }
            return $node;
        };
        $walk = static function ($node) use (&$walk, $path, $ref, $rule) {
            if (is_array($node)) {
                foreach ($node as $k => $v) $node[$k] = $walk($v);
                foreach ($rule['recursive'] ?? [] as $p => $target) $node = $path($node, explode('.', $p), $target);
            } elseif (is_string($node) && ($rule['tokens'] ?? false)) {
                $targets = ['item'=>'items', 'bot'=>'bots_info', 'world'=>'worlds', 'quest'=>'quests'];
                $node = preg_replace_callback('/<(item|bot|world|quest)_([0-9]+)>/',
                    static fn($m) => '<' . $m[1] . '_' . $ref($m[2], $targets[$m[1]]) . '>', $node);
            }
            return $node;
        };
        $original = $data[$col];
        $codec = $rule['codec'];
        if ($codec === 'id') $data[$col] = $ref($original);
        elseif ($codec === 'csv' && is_string($original)) {
            $data[$col] = preg_replace_callback('/(^|,)(\s*)([0-9]+)(?=\s*(?:,|$))/',
                static fn($m) => $m[1] . $m[2] . $ref($m[3]), $original);
        } elseif (in_array($codec, ['brackets','pairs'], true) && is_string($original) && !preg_match('/^\s*\[\s*[\[{]/', $original)) {
            $data[$col] = preg_replace_callback('/(\[\s*)([0-9]+)(?=\s*\/)/',
                static fn($m) => $m[1] . $ref($m[2]), $original);
        } elseif ($codec === 'json' || $codec === 'pairs') {
            // Decode as arrays to visit; preserve object/array shape when serializing.
            $parsed = is_string($original) ? json_decode($original, true) : $original;
            if (!is_array($parsed)) continue;
            $before = $parsed;
            if ($codec === 'pairs') {
                if (array_is_list($parsed)) foreach ($parsed as &$pair) {
                    if (!is_array($pair)) continue;
                    foreach (array_is_list($pair) ? [0] : ['id','ItemID','item_id','key'] as $key) {
                        if (array_key_exists($key, $pair)) { $pair[$key] = $ref($pair[$key]); break; }
                    }
                }
                unset($pair);
            } else {
                $parsed = $walk($parsed);
                foreach ($rule['paths'] ?? [] as $p => $target) $parsed = $path($parsed, explode('.', $p), $target);
            }
            if (!is_string($original)) $data[$col] = $parsed;
            elseif ($parsed !== $before) {
                // Apply transformed leaves to the original object tree, including empty {}.
                $shape = json_decode($original);
                $restore = static function ($shape, $value) use (&$restore) {
                    if (is_object($shape)) {
                        foreach ($shape as $k => $v) $shape->$k = $restore($v, $value[$k]);
                        return $shape;
                    }
                    if (is_array($shape)) {
                        foreach ($shape as $k => $v) $shape[$k] = $restore($v, $value[$k]);
                        return $shape;
                    }
                    return $value;
                };
                $data[$col] = json_encode($restore($shape, $parsed), JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR);
            }
        }
    }
    return $data;
}

function editor_data_references(string $resource, array $data): array
{
    $refs = [];
    editor_transform_references($resource, $data, static function ($id, $target, $column) use (&$refs) {
        $refs[] = ['id'=>$id, 'target'=>$target, 'column'=>$column];
        return $id;
    });
    return $refs;
}

function editor_remap_references(string $resource, array $data, array $idMap, bool $deferMissing = false, ?int $selfTemp = null): array
{
    $scalarColumns = array_column(array_filter(editor_reference_schema()[$resource] ?? [], static fn($rule) => $rule['codec'] === 'id'), 'column');
    return editor_transform_references($resource, $data, static function ($id, $target, $column) use ($idMap, $deferMissing, $selfTemp, $scalarColumns) {
        if (!is_guest_temp_id($id)) return $id;
        if (!array_key_exists($id, $idMap) && $deferMissing) {
            // A record's own ID is unknown until it is inserted, and columns that
            // only carry embedded references cannot block the INSERT either. Both
            // are written as "no value" now and corrected right after the INSERT.
            if ($selfTemp !== null && $id === $selfTemp) return 0;
            if (!in_array($column, $scalarColumns, true)) return 0;
        }
        if (!array_key_exists($id, $idMap)) throw new RuntimeException("Не удалось сопоставить временный ID #$id");
        return (int)$idMap[$id];
    });
}
