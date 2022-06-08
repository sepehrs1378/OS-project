# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(cache-write-coalesce) begin
(cache-write-coalesce) Create a file with 64K size
(cache-write-coalesce) Open the file!
(cache-write-coalesce) end
EOF
pass;
