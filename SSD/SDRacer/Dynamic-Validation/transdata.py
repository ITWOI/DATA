@run_command("enable-real-time-mode")
@compressName = "linebox"
@run_command("viper.console.con.input \"ftp 10.10.0.1\n\"")
@run_command("viper.console.con.input \"\n\"")
@run_command("viper.console.con.input \"bin\n\"")
@run_command("viper.console.con.input \"get "+compressName+".tar.gz\n\"")
@run_command("viper.console.con.input \"quit\n\"")

@run_command("viper.console.con.input \"tar xf "+compressName+".tar.gz\n\"")
@run_command("viper.console.con.input \"cd "+compressName+"\n\"")
@run_command("viper.console.con.input \"make\n\"")
@run_command("viper.console.con.input \"sudo insmod "+compressName+".ko\n\"")



@run_command("viper.console.con.input \"cat /proc/kallsyms > "+compressName+"Symbol\n\"")
@run_command("viper.console.con.input \"ftp 10.10.0.1\n\n\"")
@run_command("viper.console.con.input \"\n\"")
@run_command("viper.console.con.input \"put "+compressName+"Symbol\n\"")
@run_command("viper.console.con.input \"quit\n\"")



