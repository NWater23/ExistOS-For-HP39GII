{"desolve",{"Resout une equa diff","Solves a differential equation","Resuelve una ecuación diferencial",0,"Löst eine Differentialgleichung"},"","desolve(y'+x*y=0)","integrate,diff,odesolve,plotode,plotfiefd"},
{"factor",{"Factorise un polynome.","Factorizes a polynomial.","Factoriza un polinomio.",0,"Faktorisiert ein Polynom."},"Expr","factor(x^4-1)","ifactor,partfrac,normal"},
{"histogram",{"Histogramme des donnees","Data histogram","Histograma del argumento.",0,"Histogramm von Daten."},"List,min,size","histogram([1,2,1,1,2,1,2,4,3,3],0.5,1)","barplot"},
{"integrate",{"Primitive ou integrale","Indefinite or definite integral",0,0,0},"Expr,[Var(x)],[a,b])]","integrate(1/(1-x^2),x)","diff,plotarea,romberg,gaussquad"},
{"limit",{"Limite de f en x=a","Limit of f at x=a","Límite de f en x=a",0,"Limes eines Ausdrucks f an x=a"},"f,x=a,[1|-1]","limit(sin(x)/x,x,0)","series,taylor"},
{"oo",{"Infini","Infinity","Mas infinito",0,0},"",0,"infinity,-infinity"},
{"partfrac",{"Decompose elements simples","Partial fraction expansion","Descomposición parcial de una fracción",0,"Partialbruchzerlegung"},"","partfrac(x/(4-x^2))","factor,normal,convert,propfrac"},
{"plot",{"Graphe de f expression","Plot expression f","Traza la gráfica de f una expresión",0,"Plottet f einen Ausdruck"},"f","plot(x-1/6*x^3,x);","plotparam"},
{"plotfield",{"Champ des tangentes de y'=f(t,y).","Slopefield of y'=f(t,y).","Campo de las tangentes de y'=f(t,y).",0,0},"Expr,VectVar,[Opt]","plotfield(sin(t*y),[t=-3..3,y=-3..3],xstep=1,ystep=1)","plotode,desolve"},
{"plotode",{"Graphe de y'=f(t,y), y(t0)=y0","Plot of y'=f(t,y), y(t0)=y0","Solución de y'=f(t,y) con y(t0)=y0.",0,0},"Expr,Var,InitCond","plotode(sin(t*y),[t,y],[0,1])","plotfield,desolve"},
{"plotparam",{"Courbe parametrique x(t),y(t)","Parametric curve x(t),y(t)","Curva x(t),y(t)",0,"Kurve x(t),y(t)"},"x(t)+i*y(t)","plotaram(cos(2x)+i*sin(3x),x,0,pi)","plotpolar,plot"},
{"plotpolar",{"Courbe en polaire r=f(x)","Polar curve r=f(x)","Curva polar r=f(x)",0,"Polarkurve r=f(x)"},"f(x)","plotpolar(sin(2*x),x,0,pi)","plotparam,plot"},
{"plotseq",{"Graphe web d'une suite","Sequence webplot","Web-gráfica de una sucesion",0,"Webplot einer Folge"},"f(x),x=u0,n","plotseq(sqrt(2+x),6,5)","plot"},
{"product",{"Produit des valeurs","Multiplicates the values of the expression","Producto de los valores de la expresión","","Multipliziert den Wert des Ausdrucks"},"","product(n,n,1,10,2)","sum"},
{"rsolve",{"Valeur suite recurrente","Value of a recurrent sequence","Valores de una secuencia recurrente",0,"Wert einer rekursiven Folge"},"relation,u(n),u(0)","rsolve(u(n+1)=2*u(n)+3,u(n),u(0)=1)","plotseq"},
{"series",{"Developpement limite.","Series expansion","Desarrollo en serie.",0,"Serienentwicklung"},"f,x=a,ordre","series(sin(x)/x,x=0,5,polynom)","limit,taylor,polynom,truncate"},
{"simplify",{"Simplifie une expression.","Simplifies an expression.","Simplifica una expresión.",0,"Vereinfacht einen Ausdruck."},"Expression","simplify(sin(3x)/sin(x))","normal"},
{"solve",{"Résout une équation.","Solves an equation.","Resuelve una ecuación",0,"Löst eine Gleichungen."},"Equation,[Var]","solve(x^2-3=1)","linsolve,proot,fsolve,csolve,nSolve"},
{"sum",{"Somme discrete","Discrete sum","Suma discreta",0,"Diskrete Summe"},"Expr,Var,a,b","sum(1/n^2,n,1,oo)","+"},
{"tcollect",{"Rassemble sin/cos.","Collect trigonometric expressions.","Agrupa expresiones trigonométricas.",0,"Fasst trigonometrische Audrücke zusammen. "},"Expr","tcollect(sin(x)+cos(x))","texpand,tlin"},
{"texpand",{"Developpe sin/cos/tan/ln/exp.","Expand sin/cos/tan/ln/exp","Expande expresiones transcendentales.",0,"Entwickelt sin/cos/tan/ln/exp"},"Expr","texpand(sin(2*x)+exp(x+y))","tcollect,tlin,lin,trigexpand"},