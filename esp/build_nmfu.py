Import('projenv')
 
def m(target, source, env):
    target.append(str(source[0]).replace(".nmfu", ".h"))
    return (target, source)

nmfu_builder = Builder(action=Action("cd ${SOURCE.dir}; nmfu -O3 ${SOURCE.file}", "Generating parser for $SOURCE"), emitter=m, suffix=".c", src_suffix=".nmfu")

projenv.Append(BUILDERS={"Nmfu": nmfu_builder})
projenv.Nmfu("src/parser/http_serve")
