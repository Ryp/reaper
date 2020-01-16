# Make pretty images from ugly graphs
#
# deps:
# - dot (graphviz package)

TEMPLATE_DOT		= framegraph_template.dot
TEMPLATE_GEN_SCRIPT	= framegraph_fill_template.sh
PROCESSED_DOT		= framegraph.dot
FRAMEGRAPH_PNG		= framegraph.png
FRAMEGRAPH_SVG		= framegraph.svg
INPUTS				= input.txt output.txt renderpass-sideeffect.txt renderpass.txt resource.txt
OUTPUTS				= $(FRAMEGRAPH_PNG) $(FRAMEGRAPH_SVG)

all: $(OUTPUTS)

$(PROCESSED_DOT): $(INPUTS) $(TEMPLATE_DOT) $(TEMPLATE_GEN_SCRIPT)
	./$(TEMPLATE_GEN_SCRIPT) $(TEMPLATE_DOT) $(PROCESSED_DOT)

$(FRAMEGRAPH_PNG): $(PROCESSED_DOT)
	dot -Tpng -Nfontsize=11 -Nfontname=Roboto $(PROCESSED_DOT) -o $(FRAMEGRAPH_PNG)

$(FRAMEGRAPH_SVG): $(PROCESSED_DOT)
	dot -Tsvg -Nfontsize=11 -Nfontname=Roboto $(PROCESSED_DOT) -o $(FRAMEGRAPH_SVG)

clean:
	rm -f $(OUTPUTS) $(PROCESSED_DOT)
