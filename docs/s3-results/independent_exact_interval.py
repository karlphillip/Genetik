from fractions import Fraction as F


def floor_cell(scale: int, result: int):
    if scale <= 0:
        raise ValueError("positive coefficient expected")
    return F(result, scale), F(result + 1, scale)


a = floor_cell(10, 16)
b = floor_cell(20, 32)
intersection = max(a[0], b[0]), min(a[1], b[1])
assert intersection == (F(8, 5), F(33, 20))
assert (10 * intersection[0]).numerator // (10 * intersection[0]).denominator == 16
assert (20 * intersection[0]).numerator // (20 * intersection[0]).denominator == 32
assert intersection[1] == F(33, 20) and intersection[1] * 20 == 33
print(f"independent exact intersection=[{intersection[0]},{intersection[1]})")
