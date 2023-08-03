set dir [file dirname [info script]]

package ifneeded taws 0.1 [list load [file join $dir libtaws.so]]
