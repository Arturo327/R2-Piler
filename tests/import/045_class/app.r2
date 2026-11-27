module "app";
import "geom";
from "geom" import Punto;

pub fn main() : i64
{
	var a : geom::Punto = geom::Punto(3, 4);
	var b : Punto = Punto(1, 2);
	var c : geom::Punto = a + b;
	var d : geom::Punto = a.suma(b);
	var z : geom::Punto = Punto::origen();
	var r : i64 = c.get(0) + c.get(1) + a[0] + d.x + z.y;

	drop a;
	drop b;
	drop c;
	drop d;
	drop z;
	return r;
}
