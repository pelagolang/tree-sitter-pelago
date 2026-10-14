; Indentation rules in the form Zed reads: `@indent` marks a node whose inside is indented and
; `@end` the closing token that goes back.
(_ "(" ")" @end) @indent
(_ "[" "]" @end) @indent
(_ "{" "}" @end) @indent
(type_arguments "<" ">" @end) @indent
(generic_parameters "<" ">" @end) @indent
