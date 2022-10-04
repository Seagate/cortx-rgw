s/num osd = [0-9]*/num osd = 0/
s/num mds = [0-9]*/num mds = 0/
/^\[client\]$/a\\tdaos pool = tank
/^\[client\]$/a\\trgw backend store = daos
