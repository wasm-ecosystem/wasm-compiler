(module
  (func (export "multi-spilled-local") (result i64)
	(local i64)
    i64.const 10
    local.set 0
    local.get 0
    local.get 0
    block
    end
    i64.mul
    return
  )
)

(assert_return (invoke "multi-spilled-local") (i64.const 100))