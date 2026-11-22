module "a";
import "dep";
export fn ping (h : dep::Handle) : i64 {
    if (h == NULL) return 1;
    return 7;
}
