# register breakpoints

dashboard -enable off
source ./regiondump.py

break defrag()
commands
silent
dump_raw_bheap servicer.arena dump.json "echo defrag()"
cont
end

break homogenize(unsigned int)
commands
silent
dump_raw_bheap servicer.arena dump.json 'printf "homogenize(0x%03x)",slotid'
cont
end

break truncate_contents(unsigned int, unsigned int)
commands
silent
dump_raw_bheap servicer.arena dump.json 'printf "truncate_contents(0x%03x, %d)",slotid,size'
cont
end

break set_temperature(unsigned int, unsigned int)
commands
silent
dump_raw_bheap servicer.arena dump.json 'printf "set_temperature(0x%03x, %d)",slotid,temperature'
cont
end

break set_location(unsigned int, unsigned int, unsigned int, unsigned int)
commands
silent
dump_raw_bheap servicer.arena dump.json 'printf "set_location(0x%03x, 0x%04x, %d, %d)",slotid,offset,length,location'
cont
end

# break update_contents(unsigned int, unsigned int, unsigned int, void const*, bool)
# commands
# silent
# dump_raw_bheap servicer.arena dump.json 'printf "update_contents(0x%03x, 0x%04x, %d, %d)",slotid,offset,length,mark_dirty'
# cont
# end

# break ensure_single_block(unsigned int, unsigned int, unsigned int)
# commands
# silent
# dump_raw_bheap servicer.arena dump.json 'printf "ensure_single_block(%03x, %04x, %d)",slotid,offset,length'
# cont
# end

break add_block(unsigned int, unsigned int, size_t)
commands
silent
dump_raw_bheap servicer.arena dump.json 'printf "add_block(%03x, %d, %d)",slotid,datalocation,datasize'
cont
end
