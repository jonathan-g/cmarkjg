context("test-extensions")

test_that("list extensions", {
  expect_equal(list_extensions(), c("table", "strikethrough", "autolink", "tagfilter", "superscript"))
})

test_that("strikethrough", {
  md <- "foo ~~bar~~ baz"
  expect_equal(markdown_html(md, extensions = FALSE), "<p>foo ~~bar~~ baz</p>\n")
  expect_equal(markdown_html(md, extensions = TRUE), "<p>foo <del>bar</del> baz</p>\n")

  expect_equal(markdown_latex(md, extensions = FALSE), "foo \\textasciitilde{}\\textasciitilde{}bar\\textasciitilde{}\\textasciitilde{} baz\n")
  expect_equal(markdown_latex(md, extensions = TRUE), "foo \\sout{bar} baz\n")

  expect_equal(markdown_man(md, extensions = FALSE), ".PP\nfoo ~~bar~~ baz\n")
  expect_equal(markdown_man(md, extensions = TRUE), ".PP\nfoo \n.ST \"bar\"\n baz\n")

  library(xml2)
  doc1 <- xml_ns_strip(read_xml(markdown_xml(md, extensions = FALSE)))
  doc2 <- xml_ns_strip(read_xml(markdown_xml(md, extensions = TRUE)))
  expect_length(xml_find_all(doc1, "//strikethrough"), 0)
  expect_length(xml_find_all(doc2, "//strikethrough"), 1)

  md2 <- "foo ~bar~ baz"
  expect_equal(markdown_html(md2, extensions = TRUE), "<p>foo ~bar~ baz</p>\n")
  expect_equal(markdown_html(md2, max_strikethrough = TRUE, extensions = TRUE), "<p>foo <del>bar</del> baz</p>\n")

})

test_that("autolink", {
  md <- "Visit: https://www.test.com"
  expect_match(markdown_html(md, extensions = FALSE), "^((?!href).)*$", perl = TRUE)
  expect_match(markdown_html(md, extensions = TRUE), "href")

  expect_equal(markdown_latex(md, extensions = FALSE), "Visit: https://www.test.com\n")
  expect_equal(markdown_latex(md, extensions = TRUE), "Visit: \\url{https://www.test.com}\n")

  expect_equal(markdown_man(md, extensions = FALSE), ".PP\nVisit: https://www.test.com\n")
  expect_equal(markdown_man(md, extensions = TRUE), ".PP\nVisit: https://www.test.com (https://www.test.com)\n")

  library(xml2)
  doc1 <- xml_ns_strip(read_xml(markdown_xml(md, extensions = FALSE)))
  doc2 <- xml_ns_strip(read_xml(markdown_xml(md, extensions = TRUE)))
  expect_length(xml_find_all(doc1, "//link"), 0)
  expect_length(xml_find_all(doc2, "//link"), 1)

})

test_that("superscript", {
  md <- "script is^super^"
  expect_equal(markdown_html(md, extensions = FALSE), "<p>script is^super^</p>\n")
  expect_equal(markdown_html(md, extensions = TRUE), "<p>script is<sup>super</sup></p>\n")

  expect_equal(markdown_latex(md, extensions = FALSE), "script is\\^{}super\\^{}\n")
  expect_equal(markdown_latex(md, extensions = TRUE), "script is\\textsuperscript{super}\n")
})

test_that("embedded images do not get filtered", {
  md <- '<img src="data:image/png;base64,foobar" />\n'
  expect_equal(md, markdown_html(md))
  expect_equal(md, markdown_commonmark(md))
})
