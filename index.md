---
layout: home
title: seamaner
---
<h2>{{ page.title }}</h2>
<p>All blogs</p>
<ul>
       {% for post in site.posts %}
       <li>{{post.date | date_to_string }}<a href="/"></a></li>
       {% endfor %}
<ul>