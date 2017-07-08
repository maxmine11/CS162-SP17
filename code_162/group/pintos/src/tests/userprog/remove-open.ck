# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(remove-open) begin
(remove-open) open "sample.txt"
(remove-open) end
remove-open: exit(0)
EOF
pass;