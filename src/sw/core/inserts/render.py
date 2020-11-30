import json
import sys
from jinja2 import FileSystemLoader, Environment

def render_from_template(directory, template_name, d):
    loader = FileSystemLoader(directory)
    env = Environment(loader=loader)
    template = env.get_template(template_name)
    return template.render(d)

f = open(sys.argv[3])
data = json.load(f)
s = render_from_template(sys.argv[1], sys.argv[2], data)
open(sys.argv[4], "w", encoding='utf8').write(s)
