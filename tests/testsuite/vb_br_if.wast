
(module
	(func (export "const-true") (result i32)
		block
			i32.const 1
			br_if 0

			i32.const 99
			return
		end

		i32.const 11
	)

	(func (export "const-false") (result i32)
		block
			i32.const 0
			br_if 0

			i32.const 99
			return
		end

		i32.const 11
	)

	(func (export "const-true-value") (result i32)
		block (result i32)
			i32.const 7
			i32.const 1
			br_if 0

			i32.const 4
			i32.add
		end
	)

	(func (export "const-false-value") (result i32)
		block (result i32)
			i32.const 7
			i32.const 0
			br_if 0

			i32.const 4
			i32.add
		end
	)

	(func (export "const-true-return-value") (result i32)
		block
			i32.const 9
			i32.const 1
			br_if 1

			i32.const 100
			i32.add
			return
		end

		i32.const -1
	)

	(func (export "const-false-return-value") (result i32)
		block
			i32.const 9
			i32.const 0
			br_if 1

			i32.const 100
			i32.add
			return
		end

		i32.const -1
	)

	(func (export "loop-target-false") (result i32)
		loop
			i32.const 0
			br_if 0
		end

		i32.const 17
	)

	(func (export "loop-false-value") (result i32)
		block (result i32)
			loop
				i32.const 13
				i32.const 0
				br_if 1

				i32.const 7
				i32.add
				br 1
			end

			i32.const -1
		end
	)

	(func (export "nested-true-value") (result i32)
		block (result i32)
			block
				block
					i32.const 21
					i32.const 1
					br_if 2

					i32.const 5
					i32.add
					br 2
				end
			end

			i32.const -1
		end
	)

	(func (export "nested-false-value") (result i32)
		block (result i32)
			block
				block
					i32.const 21
					i32.const 0
					br_if 2

					i32.const 5
					i32.add
					br 2
				end
			end

			i32.const -1
		end
	)
)

(assert_return (invoke "const-true") (i32.const 11))
(assert_return (invoke "const-false") (i32.const 99))
(assert_return (invoke "const-true-value") (i32.const 7))
(assert_return (invoke "const-false-value") (i32.const 11))
(assert_return (invoke "const-true-return-value") (i32.const 9))
(assert_return (invoke "const-false-return-value") (i32.const 109))
(assert_return (invoke "loop-target-false") (i32.const 17))
(assert_return (invoke "loop-false-value") (i32.const 20))
(assert_return (invoke "nested-true-value") (i32.const 21))
(assert_return (invoke "nested-false-value") (i32.const 26))
