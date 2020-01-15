using NUnit.Framework;

namespace Laboratory.Tests
{
    internal class NumberLiterals : HostFixture
    {
        public NumberLiterals(IHost host) : base(host) { }
    }

/*# Integer notation
    0
    5
    -10
    +15

# Rational notation
    0.0
    5.2
    -10.86
    +3.14159

# Exponent notation
    1e0
    3E-5
    -8e7
    +2.998E8*/
}