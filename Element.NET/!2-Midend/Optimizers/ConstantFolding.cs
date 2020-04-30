namespace Element
{
	using System.Linq;
	using System.Collections.Generic;

	/// <summary>
	/// Provides functions to optimize an expression tree by evaluating
	/// parts of the tree with constant operands.
	/// </summary>
	public static class ConstantFolding
	{
		/// <summary>
		/// Optimizes an array of expressions in-place
		/// </summary>
		/// <param name="input"></param>
		public static void Optimize(Expression[] input,
			Dictionary<Expression, Expression> cache = null)
		{
			for (var i = 0; i < input.Length; i++)
			{
				input[i] = Optimize(input[i], cache);
			}
		}

		/// <summary>
		/// Optimizes a single expression, returning a new one
		/// </summary>
		/// <param name="input"></param>
		/// <returns></returns>
		public static Expression Optimize(Expression input,
			Dictionary<Expression, Expression> cache = null)
		{
			cache = cache ?? new Dictionary<Expression, Expression>();
			if (!cache.TryGetValue(input, out var retval)) {
				cache.Add(input, retval = OptimizeInternal(input, cache));
			}
			return retval;
		}

		private static Expression OptimizeInternal(Expression input,
			Dictionary<Expression, Expression> cache)
		{
			switch (input)
			{
				case Constant c: break;
				case Unary u1:
					var u1a = Optimize(u1.Operand, cache);
					var u = u1a.Equals(u1.Operand) ? u1 : new Unary(u1.Operation, u1a);
					if (u.Operand is Constant c1)
					{
						return new Constant(Unary.Evaluate(u.Operation, c1.Value));
					}
					return u;
				case Binary b1:
					var b1a = Optimize(b1.OpA, cache);
					var b1b = Optimize(b1.OpB, cache);
					var b = (b1a.Equals(b1.OpA) && b1b.Equals(b1.OpB)) ? b1 : new Binary(b1.Operation, b1a, b1b);
					var cA = (b.OpA as Constant)?.Value;
					var cB = (b.OpB as Constant)?.Value;
					if (cA.HasValue && cB.HasValue)
					{
						return new Constant(Binary.Evaluate(b.Operation, cA.Value, cB.Value));
					}
					switch (b.Operation)
					{
						case Binary.Op.Pow:
							if (cB == 0 || cA == 1) { return Constant.One; }
							if (cA == 0) { return Constant.Zero; }
							if (cB == 1) { return b.OpA; }
							if (cB == 2) { return new Binary(Binary.Op.Mul, b.OpA, b.OpA); }
							break;
						case Binary.Op.Add:
							if (cA == 0) { return b.OpB; }
							if (cB == 0) { return b.OpA; }
							break;
						case Binary.Op.Sub:
							if (cB == 0) { return b.OpA; }
							if (b.OpA.Equals(b.OpB)) { return Constant.Zero; }
							break;
						case Binary.Op.Mul:
							if (cA == 0 || cB == 0) { return Constant.Zero; }
							if (cA == 1) { return b.OpB; }
							if (cB == 1) { return b.OpA; }
							break;
						case Binary.Op.Div:
							// TODO: Divide by zero warning?
							if (cB == 1) { return b.OpA; }
							if (b.OpA.Equals(b.OpB)) { return Constant.One; }
							if (cB.HasValue) { return new Binary(Binary.Op.Mul, b.OpA, new Constant(1/cB.Value)); }
							break;
						case Binary.Op.Rem:
							// TODO: Divide by zero warning?
							if (cA == 0 || b.OpA.Equals(b.OpB)) { return Constant.Zero; }
							break;
					}
					return b;
				case Mux m1:
					var m = new Mux(Optimize(m1.Selector, cache), m1.Operands.Select(e => Optimize(e, cache)));
					if (m.Operands.Count == 1 || m.Operands.All(o => o.Equals(m.Operands[0])))
					{
						return m.Operands[0];
					}
					if (m.Selector is Constant c2)
					{
						var idx = c2.Value >= m.Operands.Count ? m.Operands.Count-1 : (c2.Value < 0 ? 0 : c2.Value);
						return m.Operands[(int)idx];
					}
					return m;
				case ExpressionGroupElement e1:
					switch (e1.Group)
					{
						// TODO: Merge these
						case Persist p:
							if (p.NewValue[e1.Index].AllDependent.All(v => (v as State)?.Scope != p.State[0].Scope))
							{
								return p.NewValue[e1.Index];
							}
							break;
						case Loop l:
							if (l.Body[e1.Index].AllDependent.All(v => (v as State)?.Scope != l.State[0].Scope))
							{
								return l.Body[e1.Index];
							}
							break;
					}
					return new ExpressionGroupElement(OptimizeGroup(e1.Group, cache), e1.Index);
			}
			return input;
		}

		public static ExpressionGroup OptimizeGroup(ExpressionGroup group,
			Dictionary<Expression, Expression> cache = null)
		{
			switch (group)
			{
				// TODO: Cut out state values that aren't used
				// TODO: Nested scopes properly
				case Persist p:
					return new Persist(
						p.State.Select(s => Optimize(s.InitialValue, cache)),
						_ => p.NewValue.Select(e => Optimize(e, cache))
					);
				case Loop l:
					return new Loop(
						l.State.Select(s => Optimize(s.InitialValue, cache)),
						_ => Optimize(l.Condition, cache),
						_ => l.Body.Select(e => Optimize(e, cache))
					);
			}
			return group;
		}
	}
}