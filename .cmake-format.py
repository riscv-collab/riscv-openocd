#  type: ignore
# pylint: skip-file

with section("format"):
    line_width = 80

with section("lint"):
    disabled_codes = [
        "C0111",  # Missing docstring on function or macro declaration
        "C0113",  # Missing {:s} in statement which allows it
    ]
