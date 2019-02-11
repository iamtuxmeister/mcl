Hooks = ["send", "preinput", "loselink", "prompt", "userinput", "connect",
"postoutput", "output", "done", "keypress"]

LastHook = 0

def hook_add(table, key, func):
  table[key] = func

def hook_remove(table, key):
  for func in table.keys():
    if table[func] is key:
      del table[func]

def hook_run(table):
  if len(table) <= 0:
    return
  for func in table.keys():
    table[func]()

def create_standard_hooks(want_run, hooks):
  for hook in hooks:
    exec(hook + "_hooks = {}\n"
         "def " + hook + "_add(data, key = None):\n"
         "  global LastHook\n"
         "  if key == None:\n"
         "    key = LastHook\n"
         "    LastHook = LastHook + 1\n"
         "  hook_add(" + hook + "_hooks, key, data)\n"
         "def " + hook + "_remove(key):\n"
         "  hook_remove(" + hook + "_hooks, key)\n", globals(), globals())
    if want_run:
      exec("def " + hook + "():\n"
           "  hook_run(" + hook + "_hooks)\n", globals(), globals())

def command():
  global default_var
  command_re_regexp = re.compile(r"^(\w+)\s*(.*)")
  command_re_match = command_re_regexp.search(default_var)  

  if command_re_match != None:
    key = string.lower(command_re_match.group(1))
    if command_hooks.has_key(key):
      tmp_def = default_var
      default_var = command_re_match.group(2)
     
      if command_hooks[key]():
        default_var = ""
      else:
        default_var = tmp_def

def cmd_show_hooks():
  for hook_group in Hooks:
    print "Hooks of type " + hook_group + ":\n"
    exec("for hook in " + hook_group + "_hooks.keys():\n"
         "  print " + hook_group + "_hooks[hook]\n")
    
create_standard_hooks(1, Hooks)
create_standard_hooks(0, ["command"])
