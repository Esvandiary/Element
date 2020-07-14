using System;
using Lexico;

namespace Element.AST
{
    [WhitespaceSurrounded, MultiLine]
    public abstract class Declaration : AstNode
    {
#pragma warning disable 649, 8618, 169
        // ReSharper disable UnassignedField.Global UnusedAutoPropertyAccessor.Local UnusedAutoPropertyAccessor.Global
        [IndirectLiteral(nameof(IntrinsicQualifier))] protected Unnamed _;
        [IndirectLiteral(nameof(Qualifier)), WhitespaceSurrounded] protected Unnamed __;
        [Term] public Identifier Identifier;
        [Optional] public PortList? PortList;
        [Optional] public ReturnConstraint? ReturnConstraint;
        [IndirectAlternative(nameof(BodyAlternatives)), WhitespaceSurrounded, MultiLine] public object Body;
        // ReSharper restore UnassignedField.Global UnusedAutoPropertyAccessor.Local UnusedAutoPropertyAccessor.Global
#pragma warning restore 649, 8618, 169

        protected abstract string IntrinsicQualifier { get; }
        protected abstract string Qualifier { get; }
        protected abstract Type[] BodyAlternatives { get; }

        public Result<IValue> Resolve(IScope scope, CompilationContext compilationContext)
        {
            compilationContext.PushDeclaration(this);
            var result = Validate(compilationContext).Bind(() => ResolveImpl(scope, compilationContext));
            compilationContext.PopDeclaration();
            return result;
        }

        protected abstract Result<IValue> ResolveImpl(IScope scope, CompilationContext context);

        protected sealed override void ValidateImpl(ResultBuilder builder, CompilationContext context)
        {
            context.PushDeclaration(this);
            ValidateDeclaration(builder, context);
            context.PopDeclaration();
        }

        protected abstract void ValidateDeclaration(ResultBuilder builder, CompilationContext context);
    }
}